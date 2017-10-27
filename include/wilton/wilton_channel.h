/* 
 * File:   wilton_channels.h
 * Author: alex
 *
 * Created on September 22, 2017, 5:51 PM
 */

#ifndef WILTON_CHANNEL_H
#define WILTON_CHANNEL_H

#include "wilton/wilton.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wilton_Channel;
typedef struct wilton_Channel wilton_Channel;

// size = 0 for sync
char* wilton_Channel_create(
        wilton_Channel** channel_out,
        int size);

// blocking
char* wilton_Channel_send(
        wilton_Channel* channel,
        const char* msg,
        int msg_len,
        int timeout_millis,
        int* success_out);

// blocking
char* wilton_Channel_receive(
        wilton_Channel* channel,
        int timeout_millis,
        char** msg_out,
        int* msg_len_out,
        int* success_out);

// non-blocking
char* wilton_Channel_offer(
        wilton_Channel* channel,
        const char* msg,
        int msg_len,
        int* success_out);

// non-blocking
char* wilton_Channel_poll(
        wilton_Channel* channel,
        char** value_out,
        int* value_len_out,
        int* success_out);

// non-blocking
char* wilton_Channel_peek(
        wilton_Channel* channel,
        char** value_out,
        int* value_len_out,
        int* success_out);

// blocking with timeout and multiplexed
char* wilton_Channel_select(
        wilton_Channel** channels,
        int channels_num,
        int timeout_millis,
        int* selected_idx_out);

char* wilton_Channel_buffered_count(
        wilton_Channel* channel,
        int* count_out);

char* wilton_Channel_max_size(
        wilton_Channel* channel,
        int* size_out);

char* wilton_Channel_close(
        wilton_Channel* channel);

#ifdef __cplusplus
}
#endif

#endif /* WILTON_CHANNEL_H */

