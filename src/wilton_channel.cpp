/* 
 * File:   wilton_channel.cpp
 * Author: alex
 *
 * Created on September 21, 2017, 9:37 PM
 */

#include "wilton/wilton_channel.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "staticlib/support.hpp"

#include "wilton/support/handle_registry.hpp"
#include "wilton/support/logging.hpp"

#include "channel.hpp"

namespace { // anonymous

const std::string LOGGER = std::string("wilton.Channel");

} // namespace

struct wilton_Channel {
private:
    wilton::channel::channel chan;

public:
    wilton_Channel(wilton::channel::channel&& chan) :
    chan(std::move(chan)) { }

    wilton::channel::channel& impl() {
        return chan;
    }
};

// size = 0 for sync
char* wilton_Channel_create(
        wilton_Channel** channel_out,
        int size) {
    if (nullptr == channel_out) return wilton::support::alloc_copy(TRACEMSG("Null 'channel_out' parameter specified"));
    if (!sl::support::is_uint32(size)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'size' parameter specified: [" + sl::support::to_string(size) + "]"));
    try {
        uint32_t size_u32 = static_cast<uint32_t> (size);
        wilton::support::log_debug(LOGGER, "Creating channel, size: [" + sl::support::to_string(size_u32) + "] ...");
        auto chan = wilton::channel::channel(size_u32);
        wilton_Channel* chan_ptr = new wilton_Channel(std::move(chan));
        wilton::support::log_debug(LOGGER, "Channel created successfully, handle: [" + wilton::support::strhandle(chan_ptr) + "]");
        *channel_out = chan_ptr;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// blocking
char* wilton_Channel_send(wilton_Channel* channel, const char* msg, int msg_len,
        int timeout_millis, int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == msg) return wilton::support::alloc_copy(TRACEMSG("Null 'msg' parameter specified"));
    if (!sl::support::is_uint32_positive(msg_len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'msg_len' parameter specified: [" + sl::support::to_string(msg_len) + "]"));
    if (!sl::support::is_uint32(timeout_millis)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'timeout_millis' parameter specified: [" + sl::support::to_string(timeout_millis) + "]"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        auto tm = std::chrono::milliseconds(static_cast<uint32_t>(timeout_millis));
        wilton::support::log_debug(LOGGER, "Sending message, handle: [" + wilton::support::strhandle(channel) + "]," +
                " message length: [" + sl::support::to_string(msg_len) + "], timeout: [" + sl::support::to_string(timeout_millis) + "] ...");
        bool success = channel->impl().send({msg, msg_len}, tm);
        wilton::support::log_debug(LOGGER, "Send complete, result: [" + sl::support::to_string_bool(success) + "]");
        *success_out = success ? 1 : 0;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// blocking
char* wilton_Channel_receive(wilton_Channel* channel, int timeout_millis,
        char** msg_out, int* msg_len_out, int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (!sl::support::is_uint32(timeout_millis)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'timeout_millis' parameter specified: [" + sl::support::to_string(timeout_millis) + "]"));
    if (nullptr == msg_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_out' parameter specified"));
    if (nullptr == msg_len_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_len_out' parameter specified"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        auto tm = std::chrono::milliseconds(static_cast<uint32_t>(timeout_millis));
        wilton::support::log_debug(LOGGER, "Receiving message, handle: [" + wilton::support::strhandle(channel) + "]," +
                " timeout: [" + sl::support::to_string(timeout_millis) + "] ...");
        auto buf = channel->impl().receive(tm);
        wilton::support::log_debug(LOGGER, "Receive complete, result: [" + sl::support::to_string_bool(buf.has_value()) + "]");
        if (buf.has_value()) {
            *msg_out = buf.value().data();
            *msg_len_out = static_cast<int> (buf.value().size());
            *success_out = true;
        } else {
            *success_out = false;
        }
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// non-blocking
char* wilton_Channel_offer(wilton_Channel* channel, const char* msg, int msg_len,
        int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == msg) return wilton::support::alloc_copy(TRACEMSG("Null 'msg' parameter specified"));
    if (!sl::support::is_uint32_positive(msg_len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'msg_len' parameter specified: [" + sl::support::to_string(msg_len) + "]"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        wilton::support::log_debug(LOGGER, "Offering message, handle: [" + wilton::support::strhandle(channel) + "]," +
                " message length: [" + sl::support::to_string(msg_len) + "] ...");
        bool success = channel->impl().offer({msg, msg_len});
        wilton::support::log_debug(LOGGER, "Offer complete, result: [" + sl::support::to_string_bool(success) + "]");
        *success_out = success ? 1 : 0;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// non-blocking
char* wilton_Channel_poll(wilton_Channel* channel, char** msg_out, int* msg_len_out,
        int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == msg_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_out' parameter specified"));
    if (nullptr == msg_len_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_len_out' parameter specified"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        wilton::support::log_debug(LOGGER, "Polling for message, handle: [" + wilton::support::strhandle(channel) + "] ...");
        auto buf = channel->impl().poll();
        wilton::support::log_debug(LOGGER, "Poll complete, result: [" + sl::support::to_string_bool(buf.has_value()) + "]");
        if (buf.has_value()) {
            *msg_out = buf.value().data();
            *msg_len_out = static_cast<int> (buf.value().size());
            *success_out = true;
        } else {
            *success_out = false;
        }
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// non-blocking
char* wilton_Channel_peek(wilton_Channel* channel, char** msg_out, int* msg_len_out,
        int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == msg_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_out' parameter specified"));
    if (nullptr == msg_len_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_len_out' parameter specified"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        wilton::support::log_debug(LOGGER, "Peeking for message, handle: [" + wilton::support::strhandle(channel) + "] ...");
        auto buf = channel->impl().peek();
        wilton::support::log_debug(LOGGER, "Peek complete, result: [" + sl::support::to_string_bool(buf.has_value()) + "]");
        if (buf.has_value()) {
            *msg_out = buf.value().data();
            *msg_len_out = static_cast<int> (buf.value().size());
            *success_out = true;
        } else {
            *success_out = false;
        }
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_Channel_select(wilton_Channel** channels, int channels_num, int timeout_millis,
        int* selected_idx_out) /* noexcept */ {
    if (nullptr == channels) return wilton::support::alloc_copy(TRACEMSG("Null 'channels' parameter specified"));
    if (!sl::support::is_uint16_positive(channels_num)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'channels_num' parameter specified: [" + sl::support::to_string(channels_num) + "]"));
    if (!sl::support::is_uint32(timeout_millis)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'timeout_millis' parameter specified: [" + sl::support::to_string(timeout_millis) + "]"));
    if (nullptr == selected_idx_out) return wilton::support::alloc_copy(TRACEMSG("Null 'selected_idx_out' parameter specified"));
    try {
        uint16_t channels_num_u16 = static_cast<uint16_t>(channels_num);
        auto vec = std::vector<std::reference_wrapper<wilton::channel::channel>>();
        auto vec_trace = std::vector<sl::json::value>();
        for (uint16_t i = 0; i < channels_num_u16; i++) {
            auto ref = std::ref(channels[i]->impl());
            vec_trace.emplace_back(wilton::support::strhandle(channels[i]));
            vec.push_back(ref);
        }
        auto tm = std::chrono::milliseconds(static_cast<uint32_t>(timeout_millis));
        wilton::support::log_debug(LOGGER, "Selecting on channels, handles: [" + sl::json::dumps(std::move(vec_trace)) + "]," +
                " timeout: [" + sl::support::to_string(timeout_millis) + "] ...");
        auto idx = wilton::channel::channel::select(vec, tm);
        wilton::support::log_debug(LOGGER, "Select complete, index: [" + sl::support::to_string(idx) + "]");
        *selected_idx_out = static_cast<int>(idx);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_Channel_buffered_count(wilton_Channel* channel, int* count_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == count_out) return wilton::support::alloc_copy(TRACEMSG("Null 'count_out' parameter specified"));
    try {
        auto size = channel->impl().queue_size();
        *count_out = static_cast<int>(size);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_Channel_max_size(wilton_Channel* channel, int* size_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == size_out) return wilton::support::alloc_copy(TRACEMSG("Null 'size_out' parameter specified"));
    try {
        auto size = channel->impl().queue_max_size();
        *size_out = static_cast<int>(size);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_Channel_close(wilton_Channel* channel) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    try {
        wilton::support::log_debug(LOGGER, "Closing channel, handle: [" + wilton::support::strhandle(channel) + "] ...");
        delete channel;
        wilton::support::log_debug(LOGGER, "Channel closed successfully");
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
