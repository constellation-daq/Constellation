#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "asio.hpp"

#include "CHIRP/config.hpp"

namespace cnstln {
namespace CHIRP {

/** Incoming broadcast message */
struct BroadcastMessage {
    /** Content of the broadcast message in bytes */
    std::vector<std::uint8_t> content;

    /** Address from which the broadcast message was received */
    asio::ip::address address;

    /** Convert the content of the broadcast message to a string */
    CHIRP_API std::string content_to_string() const;
};

/** Broadcast receiver for incoming CHIRP broadcasts on :cpp:var:`CHIRP_PORT` */
class BroadcastRecv {
public:
    /**
     * Construct broadcast receiver
     *
     * @param any_address Address for incoming broadcasts
     */
    CHIRP_API BroadcastRecv(asio::ip::address any_address = asio::ip::address_v4::any());

    /**
     * Construct broadcast receiver using human readable IP address
     *
     * @param any_ip String containing the IP for incoming broadcasts
     */
    CHIRP_API BroadcastRecv(std::string_view any_ip);

    /**
     * Receive broadcast message (blocking)
     *
     * @return Received broadcast message
     */
    CHIRP_API BroadcastMessage RecvBroadcast();

    /**
     * Receive broadcast message (asynchronously)
     *
     * @param timeout Duration for which to block function call
     * @return Broadcast message if received
     */
    CHIRP_API std::optional<BroadcastMessage> AsyncRecvBroadcast(std::chrono::steady_clock::duration timeout);

private:
    asio::io_context io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

} // namespace CHIRP
} // namespace cnstln
