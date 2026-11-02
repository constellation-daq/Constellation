/**
 * @file
 * @brief MsgPack helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <typeinfo>

#include <msgpack.hpp>

#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::utils {

    /**
     * @brief MsgPack helper function to pack value to a stream
     *
     * @throw MsgpackPackError if packing fails
     * @tparam S Reference to target stream
     * @tparam T Value to be packed
     */
    template <typename S, typename T> inline void msgpack_pack(S& stream, const T& object) {
        try {
            msgpack::pack(stream, object);
        } catch(const msgpack::type_error& e) {
            throw MsgpackPackError("Type error for " + utils::demangle<T>(), e.what());
        } catch(const msgpack::parse_error& e) {
            throw MsgpackPackError("Error parsing data", e.what());
        }
    }

    /**
     * @brief MsgPack helper to unpack a value to target type
     * @note Unpacking is limited to the buffer length, the nesting depth limit is hardcoded to 64
     *
     * @throw MsgpackUnpackError if unpacking fails
     * @tparam R Target return type
     * @param data Pointer to the buffer
     * @param len Length of the buffer
     * @param offset Offset in the buffer, updated to the position after the unpacked value
     * @return Unpacked value in target type
     */
    template <typename R> inline R msgpack_unpack_to(const char* data, std::size_t len, std::size_t& offset) {
        try {
            const auto limit = msgpack::unpack_limit(len, len, len, len, len, 64);
            const auto msgpack_var = msgpack::unpack(data, len, offset, nullptr, nullptr, limit);
            return msgpack_var->template as<R>();
        } catch(const msgpack::type_error& e) {
            throw MsgpackUnpackError("Type error for " + utils::demangle<R>(), e.what());
        } catch(const std::bad_cast& e) { // Workaround for libc++: same as msgpack::type_error
            throw MsgpackUnpackError("Type error for " + utils::demangle<R>(), e.what());
        } catch(const msgpack::parse_error& e) {
            throw MsgpackUnpackError("Error parsing data", e.what());
        } catch(const msgpack::unpack_error& e) {
            throw MsgpackUnpackError("Error unpacking data", e.what());
        }
    }

    /**
     * @brief MsgPack helper to unpack a value to target type from the start of a buffer
     *
     * @throw MsgpackUnpackError if unpacking fails
     * @tparam R Target return type
     * @param data Pointer to the buffer
     * @param len Length of the buffer
     * @return Unpacked value in target type
     */
    template <typename R> inline R msgpack_unpack_to(const char* data, std::size_t len) {
        std::size_t offset = 0;
        return msgpack_unpack_to<R>(data, len, offset);
    }

    /**
     * @brief MsgPack helper to unpack an enum
     *
     * @throw MsgpackUnpackError if unpacking fails
     * @tparam R Target return type
     * @tparam Args Variadic arguments
     * @return Unpacked value in target type
     */
    template <typename R, typename... Args>
        requires std::is_enum_v<R>
    inline R msgpack_unpack_to_enum(Args&&... args) {
        const auto enum_opt = enum_cast<R>(msgpack_unpack_to<std::underlying_type_t<R>>(std::forward<Args>(args)...));
        if(!enum_opt.has_value()) {
            throw MsgpackUnpackError("Type error for " + utils::demangle<R>(), "value out of range");
        }
        return enum_opt.value();
    }

} // namespace constellation::utils
