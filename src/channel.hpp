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
 * File:   channel.hpp
 * Author: alex
 *
 * Created on September 21, 2017, 10:35 PM
 */

#ifndef WILTON_CHANNEL_CHANNEL_HPP
#define WILTON_CHANNEL_CHANNEL_HPP

#include <cstdint>
#include <chrono>
#include <functional>
#include <vector>

#include "staticlib/io.hpp"
#include "staticlib/pimpl.hpp"

#include "wilton/support/buffer.hpp"
#include "wilton/support/exception.hpp"

namespace wilton {
namespace channel {

class channel : public sl::pimpl::object {
protected:
    /**
     * implementation class
     */
    class impl;
    
public:
    /**
     * PIMPL-specific constructor
     * 
     * @param pimpl impl object
     */
    PIMPL_CONSTRUCTOR(channel)

    channel(uint32_t size);

    bool send(sl::io::span<const char> msg, std::chrono::milliseconds timeout);

    support::buffer receive(std::chrono::milliseconds timeout);
    
    bool offer(sl::io::span<const char> msg);
    
    support::buffer poll();

    support::buffer peek();

    uint32_t queue_size();

    uint32_t queue_max_size();

    static int32_t select(std::vector<std::reference_wrapper<channel>>& channels,
            std::chrono::milliseconds timeout);

    static void initialize();

    inline int64_t instance_id() {
        return reinterpret_cast<int64_t>(this->get_impl_ptr().get());
    }

};

} // namespace
}


#endif /* WILTON_CHANNEL_CHANNEL_HPP */

