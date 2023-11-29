#pragma once

#include <string_view>

#include "asio.hpp"

#include "CHIRP/config.hpp"

namespace cnstln {
namespace CHIRP {

/** Broadcast sender for outgoing CHIRP broadcasts on :cpp:var:`CHIRP_PORT` */
class BroadcastSend {
public:
    /**
     * Construct broadcast sender
     *
     * @param brd_address Broadcast address for outgoing broadcasts
     */
    CHIRP_API BroadcastSend(asio::ip::address brd_address = asio::ip::address_v4::any());

    /**
     * Construct broadcast sender using human readable IP address
     *
     * @param brd_ip String containing the broadcast IP for outgoing broadcasts
     */
    CHIRP_API BroadcastSend(std::string_view brd_ip);

    /**
     * Send broadcast message from string
     *
     * @param message String with broadcast message
     */
    CHIRP_API void SendBroadcast(std::string_view message);

    /**
     * Send broadcast message
     *
     * @param data Pointer to message data
     * @param size Message length in bytes
     */
    CHIRP_API void SendBroadcast(const void* data, std::size_t size);

private:
    asio::io_context io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

} // namespace CHIRP
} // namespace cnstln
