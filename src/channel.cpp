/*
 * Copyright 2017, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   channel.cpp
 * Author: alex
 *
 * Created on September 21, 2017, 10:35 PM
 */

#include "channel.hpp"

#include <condition_variable>
#include <deque>
#include <list>
#include <memory>
#include <mutex>

#include "staticlib/pimpl/forward_macros.hpp"
#include "staticlib/utils.hpp"

namespace wilton {
namespace channel {

namespace { // anonymous

class selector {
public:
    std::shared_ptr<std::condition_variable> cv;
    int64_t channel_id;
    bool selected;

    selector(std::shared_ptr<std::condition_variable> cv, int64_t channel_id, bool selected) :
    cv(cv),
    channel_id(channel_id),
    selected(selected) { }
};

std::shared_ptr<std::mutex> shared_mutex() {
    static auto mutex = std::make_shared<std::mutex>();
    return mutex;
}

// todo: think about proper indexing instead of o(n) access
std::shared_ptr<std::list<selector>> shared_selectors() {
    static auto list = std::make_shared<std::list<selector>>();
    return list;
}

} // namespace

class channel::impl : public staticlib::pimpl::object::impl {
    std::shared_ptr<std::mutex> mutex;
    std::condition_variable empty_cv;
    std::condition_variable full_cv;
    std::condition_variable sync_cv;
    std::deque<std::string> queue;

    uint32_t max_size;
    bool unblocked = false;

public:
    impl(uint32_t size) :
    mutex(shared_mutex()),
    max_size(size) { }

    ~impl() STATICLIB_NOEXCEPT {
        std::unique_lock<std::mutex> guard{*mutex};
        this->unblocked = true;
        empty_cv.notify_all();
        full_cv.notify_all();
        sync_cv.notify_all();
    }

    bool send(channel& frontend, sl::io::span<const char> msg, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> guard{*mutex};
        if (unblocked) {
            return false;
        }
        int64_t cid = frontend.instance_id();
        return max_size > 0 ? send_buffered(cid, guard, msg, timeout) : send_sync(cid, guard, msg, timeout);
    }

    support::buffer receive(channel&, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> guard{*mutex};
        if (unblocked) {
            return support::make_null_buffer();
        }
        if (queue.size() > 0) {
            return pop_queue();
        } else {
            auto predicate = [this] {
                return this->unblocked || queue.size() > 0;
            };
            if(std::chrono::milliseconds(0) == timeout) {
                empty_cv.wait(guard, predicate);
            } else {
                empty_cv.wait_for(guard,timeout, predicate);
            }
            if (unblocked || 0 == queue.size()) {
                return support::make_null_buffer();
            }
            return pop_queue();
        }
    }
    
    bool offer(channel& frontend, sl::io::span<const char> msg) {
        std::lock_guard<std::mutex> guard{*mutex};
        if (unblocked || 0 == max_size) {
            return false;
        }
        if (queue.size() < max_size) {
            return push_queue(frontend.instance_id(), msg);
        } else {
            return false;
        }
    }
    
    support::buffer poll(channel&) {
        std::lock_guard<std::mutex> guard{*mutex};
        if (unblocked || 0 == max_size) {
            return support::make_null_buffer();
        }
        if (queue.size() > 0) {
            return pop_queue();
        } else {
            return support::make_null_buffer();
        }
    }

    support::buffer peek(channel&) {
        std::lock_guard<std::mutex> guard{*mutex};
        if (unblocked) {
            return support::make_null_buffer();
        }
        if (queue.size() > 0) {
            return support::make_string_buffer(queue.front());
        } else {
            return support::make_null_buffer();
        }
    }

    uint32_t queue_size(channel&) {
        std::lock_guard<std::mutex> guard{*mutex};
        auto res = 0 == max_size ? 0 : queue.size(); 
        return static_cast<uint32_t>(res);
    }

    uint32_t queue_max_size(channel&) {
        return static_cast<uint32_t>(max_size);
    }

    static int32_t select(std::vector<std::reference_wrapper<channel>> channels,
            std::chrono::milliseconds timeout) {
        auto mx = shared_mutex();
        std::unique_lock<std::mutex> guard{*mx};
        auto sels = shared_selectors();
        auto cv = std::make_shared<std::condition_variable>();
        // add selectors
        for (auto ch : channels) {
            auto ptr = reinterpret_cast<impl*>(ch.get().get_impl_ptr().get());
            bool selected = ptr->queue.size() > 0;
            sels->emplace_back(cv, ch.get().instance_id(), selected);
        }
        int64_t selected_id = -1;
        auto predicate = [&sels, &cv, &selected_id] {
            for (auto& en : *sels) {
                if (en.cv.get() == cv.get() && en.selected) {
                    selected_id = en.channel_id;
                    return true;
                }
            }
            return false;
        };
        // wait for pushes
        if(std::chrono::milliseconds(0) == timeout) {
            cv->wait(guard, predicate);
        } else {
            cv->wait_for(guard, timeout, predicate);
        }
        // remove selectors
        sels->remove_if([&cv] (selector& en) {
            return en.cv.get() == cv.get();
        });
        // convert selected channel_id (if selected) to list index
        if (-1 != selected_id) {
            for (size_t i = 0; i < channels.size(); i++) {
                if (channels.at(i).get().instance_id() == selected_id) {
                    return static_cast<int32_t>(i);
                }
            }
            throw support::exception(TRACEMSG(
                    "Invalid selectors state, selected channel not found, id: [" + sl::support::to_string(selected_id) + "]"));
        } else {
            return -1;
        }
    }

private:

    bool send_buffered(int64_t channel_id, std::unique_lock<std::mutex>& guard,
            sl::io::span<const char> msg, std::chrono::milliseconds timeout) {
        if (queue.size() < max_size) {
            return push_queue(channel_id, msg);
        } else {
            auto predicate = [this] {
                return this->unblocked || queue.size() < max_size;
            };
            if(std::chrono::milliseconds(0) == timeout) {
                full_cv.wait(guard, predicate);
            } else {
                full_cv.wait_for(guard, timeout, predicate);
            }
            if (unblocked || queue.size() == max_size) {
                return false;
            }
            return push_queue(channel_id, msg);
        }
    }

    bool send_sync(int64_t channel_id, std::unique_lock<std::mutex>& guard,
            sl::io::span<const char> msg, std::chrono::milliseconds timeout) {
        auto predicate = [this] {
            return this->unblocked || 0 == queue.size();
        };
        auto awaited = std::chrono::milliseconds(0);
        if (0 == queue.size()) {
            push_queue(channel_id, msg);
        } else if (1 == queue.size()) {
            if(std::chrono::milliseconds(0) == timeout) {
                full_cv.wait(guard, predicate);
            } else {
                auto start = std::chrono::milliseconds(sl::utils::current_time_millis_steady());
                full_cv.wait_for(guard, timeout, predicate);
                awaited = std::chrono::milliseconds(sl::utils::current_time_millis_steady()) - start;
            }
            if (unblocked || 0 != queue.size()) {
                return false;
            }
            push_queue(channel_id, msg);
        } else throw support::exception(TRACEMSG(
                "Invalid state detected for sync channel, queue size: [" + sl::support::to_string(queue.size()) + "]"));
        // await received
        if(std::chrono::milliseconds(0) == timeout) {
            sync_cv.wait(guard, predicate);
        } else {
            auto timeout_left = timeout - awaited;
            if (timeout_left > std::chrono::milliseconds(0)) {
                sync_cv.wait_for(guard, timeout_left, predicate);
            }
        }
        return !unblocked && 0 == queue.size();
    }

    bool push_queue(int64_t channel_id, sl::io::span<const char> msg) {
        queue.emplace_back(msg.data(), msg.size());
        if (1 == queue.size()) {
            empty_cv.notify_all();
            auto sels = shared_selectors();
            for (auto& en : *sels) {
                if (en.channel_id == channel_id) {
                    en.selected = true;
                    en.cv->notify_all();
                    break;
                }
            }
        }
        return true;
    }
    
    support::buffer pop_queue() {
        auto res = support::make_string_buffer(queue.front());
        queue.pop_front();
        if (0 != max_size) {
            if (max_size - 1 == queue.size()) {
                full_cv.notify_all();
            }
        } else if (0 == queue.size()) {
            full_cv.notify_all();
            sync_cv.notify_all();
        } else throw support::exception(TRACEMSG(
                "Invalid state detected for sync channel, queue size: [" + sl::support::to_string(queue.size()) + "]"));
        return res;
    }
};
PIMPL_FORWARD_CONSTRUCTOR(channel, (uint32_t), (), support::exception)
PIMPL_FORWARD_METHOD(channel, bool, send, (sl::io::span<const char>)(std::chrono::milliseconds), (), support::exception)
PIMPL_FORWARD_METHOD(channel, support::buffer, receive, (std::chrono::milliseconds), (), support::exception)
PIMPL_FORWARD_METHOD(channel, bool, offer, (sl::io::span<const char>), (), support::exception)
PIMPL_FORWARD_METHOD(channel, support::buffer, poll, (), (), support::exception)
PIMPL_FORWARD_METHOD(channel, support::buffer, peek, (), (), support::exception)
PIMPL_FORWARD_METHOD(channel, uint32_t, queue_size, (), (), support::exception)
PIMPL_FORWARD_METHOD(channel, uint32_t, queue_max_size, (), (), support::exception)
PIMPL_FORWARD_METHOD_STATIC(channel, int32_t, select, (std::vector<std::reference_wrapper<channel>>&)(std::chrono::milliseconds), (), support::exception)

} // namespace
}
