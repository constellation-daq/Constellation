#pragma once

#include <cstdint>

namespace cnstln {
namespace CHIRP {

// Version of CHIRP protocol
constexpr std::uint8_t CHIRP_VERSION = '\x01';

// Port number of the CHIRP protocol
constexpr std::uint16_t CHIRP_PORT = 7123;

// CHIRP Message length in bytes
constexpr std::size_t MESSAGE_LENGTH = 42;

}
}
