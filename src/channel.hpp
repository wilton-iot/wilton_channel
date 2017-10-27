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

    inline int64_t instance_id() {
        return reinterpret_cast<int64_t>(this->get_impl_ptr().get());
    }

};

} // namespace
}


#endif /* WILTON_CHANNEL_CHANNEL_HPP */

