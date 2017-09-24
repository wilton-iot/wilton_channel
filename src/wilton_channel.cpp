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

#include "channel.hpp"

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
        auto chan = wilton::channel::channel(size_u32);
        wilton_Channel* chan_ptr = new wilton_Channel(std::move(chan));
        *channel_out = chan_ptr;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// blocking
char* wilton_Channel_send(wilton_Channel* channel, const char* msg, int msg_len,
        int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == msg) return wilton::support::alloc_copy(TRACEMSG("Null 'msg' parameter specified"));
    if (!sl::support::is_uint32_positive(msg_len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'msg_len' parameter specified: [" + sl::support::to_string(msg_len) + "]"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        bool success = channel->impl().send({msg, msg_len});
        *success_out = success ? 1 : 0;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

// blocking
char* wilton_Channel_receive(wilton_Channel* channel, char** msg_out, int* msg_len_out,
        int* success_out) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    if (nullptr == msg_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_out' parameter specified"));
    if (nullptr == msg_len_out) return wilton::support::alloc_copy(TRACEMSG("Null 'msg_len_out' parameter specified"));
    if (nullptr == success_out) return wilton::support::alloc_copy(TRACEMSG("Null 'success_out' parameter specified"));
    try {
        auto buf = channel->impl().receive();
        if (buf.has_value()) {
            *msg_out = buf.value().data();
            *msg_len_out = buf.value().size();
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
        bool success = channel->impl().offer({msg, msg_len});
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
        auto buf = channel->impl().poll();
        if (buf.has_value()) {
            *msg_out = buf.value().data();
            *msg_len_out = buf.value().size();
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
        for (uint16_t i = 0; i < channels_num_u16; i++) {
            auto ref = std::ref(channels[i]->impl());
            vec.push_back(ref);
        }
        auto tm = std::chrono::milliseconds(static_cast<uint32_t>(timeout_millis));
        auto idx = wilton::channel::channel::select(vec, tm);
        *selected_idx_out = static_cast<int>(idx);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_Channel_buffered_count(wilton_Channel* channel, int* count_out) {
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

char* wilton_Channel_close(wilton_Channel* channel) /* noexcept */ {
    if (nullptr == channel) return wilton::support::alloc_copy(TRACEMSG("Null 'channel' parameter specified"));
    try {
        delete channel;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
