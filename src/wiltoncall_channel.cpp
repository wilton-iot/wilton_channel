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
 * File:   wiltoncall_channel.cpp
 * Author: alex
 *
 * Created on September 21, 2017, 10:31 PM
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "wilton/wilton_channel.h"

#include "staticlib/io.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/buffer.hpp"
#include "wilton/support/exception.hpp"
#include "wilton/support/handle_registry.hpp"
#include "wilton/support/registrar.hpp"

namespace wilton {
namespace channel {

namespace { //anonymous

std::shared_ptr<support::handle_registry<wilton_Channel>> shared_registry() {
    static auto registry = std::make_shared<support::handle_registry<wilton_Channel>>(
        [] (wilton_Channel* chan) STATICLIB_NOEXCEPT {
            wilton_Channel_close(chan);
        });
    return registry;
}

std::shared_ptr<std::unordered_map<std::string, int64_t>> shared_lookup_map() {
    static auto map = std::make_shared<std::unordered_map<std::string, int64_t>>();
    return map;
}

} // namespace

support::buffer create(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto rname = std::ref(sl::utils::empty_string());
    int64_t size = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("name" == name) {
            rname = fi.as_string_nonempty_or_throw(name);
        } else if ("size" == name) {
            size = fi.as_uint32_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (rname.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'name' not specified"));
    const std::string& name = rname.get();
    if (-1 == size) throw support::exception(TRACEMSG(
            "Required parameter 'size' not specified"));
    // take registry lock and proceed with checks, creation and registering
    auto reg = shared_registry();
    std::lock_guard<std::mutex> guard{reg->mutex()};
    // check duplicate
    auto lm = shared_lookup_map();
    auto count = lm->count(name);
    if (count > 0) throw support::exception(TRACEMSG(
            "Channel with specified name already exists, name: [" + name + "]"));
    // call wilton
    wilton_Channel* chan = nullptr;
    char* err = wilton_Channel_create(std::addressof(chan), static_cast<int> (size));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    int64_t handle = reg->put_nolock(chan);
    lm->emplace(name, handle);
    return support::make_json_buffer({
        { "channelHandle", handle}
    });
}

support::buffer lookup(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto rname = std::ref(sl::utils::empty_string());
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("name" == name) {
            rname = fi.as_string_nonempty_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (rname.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'name' not specified"));
    const std::string& name = rname.get();
    // check lookup registry
    auto reg = shared_registry();
    std::lock_guard<std::mutex> guard{reg->mutex()};
    auto lm = shared_lookup_map();
    auto it = lm->find(name);
    if (it == lm->end()) throw support::exception(TRACEMSG(
            "Channel with specified name not found, name: [" + name +"]"));
    return support::make_json_buffer({
        { "channelHandle", it->second }
    });
}

support::buffer send(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rmsg = std::ref(sl::utils::empty_string());
    int64_t timeout = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("message" == name) {
            rmsg = fi.as_string_nonempty_or_throw(name);
        } else if ("timeoutMillis" == name) {
            timeout = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    if (rmsg.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'message' not specified"));
    if (-1 == timeout) throw support::exception(TRACEMSG(
            "Required parameter 'timeoutMillis' not specified"));
    const std::string& msg = rmsg.get();
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    int success = -1;
    auto err = wilton_Channel_send(chan, msg.c_str(), static_cast<int>(msg.length()),
            static_cast<int>(timeout), std::addressof(success));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_json_buffer({
        { "success", 1 == success }
    });
}

support::buffer receive(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    int64_t timeout = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("timeoutMillis" == name) {
            timeout = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    if (-1 == timeout) throw support::exception(TRACEMSG(
            "Required parameter 'timeoutMillis' not specified"));
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    char* msg_out = nullptr;
    int msg_len_out = -1;
    int success = -1;
    auto err = wilton_Channel_receive(chan, static_cast<int>(timeout), std::addressof(msg_out), 
            std::addressof(msg_len_out), std::addressof(success));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    if (1 == success) {
        return support::wrap_wilton_buffer(msg_out, msg_len_out);
    } else {
        return support::make_empty_buffer();
    }
}

support::buffer offer(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rmsg = std::ref(sl::utils::empty_string());
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("message" == name) {
            rmsg = fi.as_string_nonempty_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    if (rmsg.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'message' not specified"));
    const std::string& msg = rmsg.get();
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    int success = -1;
    auto err = wilton_Channel_offer(chan, msg.c_str(), static_cast<int>(msg.length()),
            std::addressof(success));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_json_buffer({
        { "success", 1 == success }
    });
}

support::buffer poll(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    char* msg_out = nullptr;
    int msg_len_out = -1;
    int success = -1;
    auto err = wilton_Channel_poll(chan, std::addressof(msg_out), 
            std::addressof(msg_len_out), std::addressof(success));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    if (1 == success) {
        return support::wrap_wilton_buffer(msg_out, msg_len_out);
    } else {
        return support::make_empty_buffer();
    }
}

support::buffer peek(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    char* msg_out = nullptr;
    int msg_len_out = -1;
    int success = -1;
    auto err = wilton_Channel_peek(chan, std::addressof(msg_out), 
            std::addressof(msg_len_out), std::addressof(success));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    if (1 == success) {
        return support::wrap_wilton_buffer(msg_out, msg_len_out);
    } else {
        return support::make_empty_buffer();
    }
}

support::buffer select(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto handles = std::vector<int64_t>();
    int64_t timeout = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channels" == name) {
            auto& vec = fi.as_array_or_throw(name);
            for (auto& val : vec) {
                int64_t ha = val.as_int64_or_throw(name);
                handles.push_back(ha);
            }
        } else if ("timeoutMillis" == name) {
            timeout = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (handles.empty()) throw support::exception(TRACEMSG(
            "Required parameter 'channels' not specified"));
    if (-1 == timeout) throw support::exception(TRACEMSG(
            "Required parameter 'timeoutMillis' not specified"));
    // get handles
    auto reg = shared_registry();
    auto channels = std::vector<wilton_Channel*>();
    for (int64_t ha : handles) {
        auto chan = reg->peek(ha);
        if (nullptr == chan) throw support::exception(TRACEMSG(
                "Invalid 'channelHandle' parameter specified: [" + sl::support::to_string(ha) + "]"));
        channels.push_back(chan);
    }
    // call wilton
    int selected_idx_out = -1;
    auto err = wilton_Channel_select(channels.data(), static_cast<int>(channels.size()), 
            static_cast<int>(timeout), std::addressof(selected_idx_out));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_json_buffer({
        { "selectedChannelIndex", selected_idx_out }
    });
}

support::buffer close(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->remove(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    char* err = wilton_Channel_close(chan);
    if (nullptr != err) {
        reg->put(chan);
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    // pop registry
    std::lock_guard<std::mutex> guard{reg->mutex()};
    auto lm = shared_lookup_map();
    auto rname = std::ref(sl::utils::empty_string());
    for (auto& pa : *lm) {
        if (handle == pa.second) {
            rname = pa.first;
            break;
        }
    }
    if (rname.get().empty()) throw support::exception(TRACEMSG(
            "Registry cleanup error, specified channel not found"));
    auto removed = lm->erase(rname.get());
    if (1 != removed) throw support::exception(TRACEMSG(
            "Registry cleanup error, 'erase' failed, name: [" + rname.get() + "]"));
    return support::make_empty_buffer();
}

support::buffer get_max_size(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // call wilton
    int size = -1;
    char* err = wilton_Channel_max_size(chan, std::addressof(size));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_json_buffer({
        { "maxSize", size }
    });
}

support::buffer get_name(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("channelHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'channelHandle' not specified"));
    // get handle
    auto reg = shared_registry();
    wilton_Channel* chan = reg->peek(handle);
    if (nullptr == chan) throw support::exception(TRACEMSG(
            "Invalid 'channelHandle' parameter specified"));
    // get registry
    auto map_copy = [&reg] {
        std::lock_guard<std::mutex> guard{reg->mutex()};
        return *shared_lookup_map();
    } ();
    // find name
    for (auto& pa : map_copy) {
        if (handle == pa.second) {
            return support::make_string_buffer(pa.first);
        }
    }
    throw support::exception(TRACEMSG("Channel not found, handle: [" + sl::support::to_string(handle) + "]"));
}

support::buffer dump_registry(sl::io::span<const char>) {
    auto reg = shared_registry();
    auto map_copy = [&reg] {
        std::lock_guard<std::mutex> guard{reg->mutex()};
        return *shared_lookup_map();
    } ();
    auto vec = std::vector<sl::json::value>();
    for (auto& pa : map_copy) {
        // get handle
        wilton_Channel* chan = reg->peek(pa.second);
        if (nullptr == chan) throw support::exception(TRACEMSG(
                "Invalid 'channelHandle' parameter specified, handle: [" + sl::support::to_string(pa.second) + "]"));
        // call wilton
        int count = -1;
        char* err = wilton_Channel_buffered_count(chan, std::addressof(count));
        if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
        auto fields = std::vector<sl::json::field>();
        fields.emplace_back("name", pa.first);
        fields.emplace_back("handle", pa.second);
        fields.emplace_back("bufferedMessagesCount", count);
        vec.emplace_back(std::move(fields));
    }
    return support::make_json_buffer(std::move(vec));
}

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        wilton::support::register_wiltoncall("channel_create", wilton::channel::create);
        wilton::support::register_wiltoncall("channel_lookup", wilton::channel::lookup);
        wilton::support::register_wiltoncall("channel_send", wilton::channel::send);
        wilton::support::register_wiltoncall("channel_receive", wilton::channel::receive);
        wilton::support::register_wiltoncall("channel_offer", wilton::channel::offer);
        wilton::support::register_wiltoncall("channel_poll", wilton::channel::poll);
        wilton::support::register_wiltoncall("channel_peek", wilton::channel::peek);
        wilton::support::register_wiltoncall("channel_select", wilton::channel::select);
        wilton::support::register_wiltoncall("channel_close", wilton::channel::close);
        wilton::support::register_wiltoncall("channel_get_max_size", wilton::channel::get_max_size);
        wilton::support::register_wiltoncall("channel_get_name", wilton::channel::get_name);
        wilton::support::register_wiltoncall("channel_dump_registry", wilton::channel::dump_registry);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
