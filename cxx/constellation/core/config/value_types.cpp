/**
 * @file
 * @brief Implementation of configuration value types
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "value_types.hpp"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <msgpack.hpp>

#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;

// NOLINTBEGIN(misc-no-recursion)

// --- Scalar ---

std::string Scalar::to_string() const {
    return std::visit(
        [](const auto& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                return "NIL";
            } else {
                return ::to_string(arg);
            }
        },
        *this);
}

std::string Scalar::demangle() const {
    return std::visit(
        [](const auto& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                return "NIL";
            } else {
                return utils::demangle<T>();
            }
        },
        *this);
}

void Scalar::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    std::visit(
        [&](const auto& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                msgpack_packer.pack_nil();
            } else {
                msgpack_packer.pack(arg);
            }
        },
        *this);
}

void Scalar::msgpack_unpack(const msgpack::object& msgpack_object) {
    switch(msgpack_object.type) {
    case msgpack::type::BOOLEAN: {
        emplace<bool>(msgpack_object.as<bool>());
        break;
    }
    case msgpack::type::POSITIVE_INTEGER:
    case msgpack::type::NEGATIVE_INTEGER: {
        emplace<std::int64_t>(msgpack_object.as<std::int64_t>());
        break;
    }
    case msgpack::type::FLOAT32:
    case msgpack::type::FLOAT64: {
        emplace<double>(msgpack_object.as<double>());
        break;
    }
    case msgpack::type::STR: {
        emplace<std::string>(msgpack_object.as<std::string>());
        break;
    }
    case msgpack::type::EXT: {
        // Try to convert to time_point, throws if wrong EXT type
        emplace<std::chrono::system_clock::time_point>(msgpack_object.as<std::chrono::system_clock::time_point>());
        break;
    }
    case msgpack::type::NIL: {
        // If NIL, then leave variant at default-constructed std::monostate
        break;
    }
    default: {
        throw msgpack::type_error();
    }
    }
}

// --- Array ---

std::string Array::to_string() const {
    return "[" +
           std::visit(
               [](const auto& arg) -> std::string {
                   using T = std::decay_t<decltype(arg)>;
                   if constexpr(std::same_as<T, std::monostate>) {
                       return "";
                   } else {
                       return " " + utils::range_to_string(arg) + " ";
                   }
               },
               *this) +
           "]";
}

std::string Array::demangle() const {
    return std::visit(
        [](const auto& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                return "Array";
            } else {
                using U = T::value_type;
                return "Array<" + utils::demangle<U>() + ">";
            }
        },
        *this);
}

void Array::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    std::visit(
        [&](const auto& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                msgpack_packer.pack_array(0);
            } else {
                msgpack_packer.pack(arg);
            }
        },
        *this);
}

namespace {
    template <typename T> std::vector<T> array_msgpack_unpack(std::span<msgpack::object> msgpack_array) {
        std::vector<T> out {};
        out.reserve(msgpack_array.size());
        for(const auto& msgpack_object : msgpack_array) {
            out.emplace_back(msgpack_object.as<T>());
        }
        return out;
    }
} // namespace

void Array::msgpack_unpack(const msgpack::object& msgpack_object) {
    // Unpack array
    if(msgpack_object.type != msgpack::type::ARRAY) [[unlikely]] {
        throw msgpack::type_error();
    }
    const auto msgpack_array_raw = msgpack_object.via.array; // NOLINT(cppcoreguidelines-pro-type-union-access)
    const auto msgpack_array = std::span(msgpack_array_raw.ptr, msgpack_array_raw.size);

    if(msgpack_array.empty()) {
        emplace<std::monostate>();
    } else {
        const auto msgpack_type = msgpack_array[0].type;
        switch(msgpack_type) {
        case msgpack::type::BOOLEAN: {
            emplace<std::vector<bool>>(array_msgpack_unpack<bool>(msgpack_array));
            break;
        }
        case msgpack::type::POSITIVE_INTEGER:
        case msgpack::type::NEGATIVE_INTEGER: {
            emplace<std::vector<std::int64_t>>(array_msgpack_unpack<std::int64_t>(msgpack_array));
            break;
        }
        case msgpack::type::FLOAT32:
        case msgpack::type::FLOAT64: {
            emplace<std::vector<double>>(array_msgpack_unpack<double>(msgpack_array));
            break;
        }
        case msgpack::type::STR: {
            emplace<std::vector<std::string>>(array_msgpack_unpack<std::string>(msgpack_array));
            break;
        }
        case msgpack::type::EXT: {
            // Try to convert to time_point, throws if wrong EXT type
            emplace<std::vector<std::chrono::system_clock::time_point>>(
                array_msgpack_unpack<std::chrono::system_clock::time_point>(msgpack_array));
            break;
        }
        default: {
            throw msgpack::type_error();
        }
        }
    }
}

// --- Dictionary ---

std::string Dictionary::to_string() const {
    std::string out = "{";
    if(!empty()) {
        out += " " + range_to_string(*this, [](const auto& p) { return p.first + ": " + p.second.to_string(); }) + " ";
    }
    out += "}";
    return out;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::string Dictionary::demangle() const {
    return "Dictionary";
}

void Dictionary::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    msgpack_packer.pack_map(size());
    for(auto const& [key, val] : *this) {
        msgpack_packer.pack(key);
        msgpack_packer.pack(val);
    }
}

void Dictionary::msgpack_unpack(const msgpack::object& msgpack_object) {
    // Unpack map
    if(msgpack_object.type != msgpack::type::MAP) [[unlikely]] {
        throw msgpack::type_error();
    }
    const auto msgpack_map_raw = msgpack_object.via.map; // NOLINT(cppcoreguidelines-pro-type-union-access)
    const auto msgpack_map = std::span(msgpack_map_raw.ptr, msgpack_map_raw.size);

    // Unpack and insert in key-value pairs into the map
    for(const auto& msgpack_kv : msgpack_map) {
        insert_or_assign(msgpack_kv.key.as<std::string>(), msgpack_kv.val.as<Composite>());
    }
}

PayloadBuffer Dictionary::assemble() const {
    msgpack::sbuffer sbuf {};
    ::msgpack_pack(sbuf, *this);
    return {std::move(sbuf)};
}

Dictionary Dictionary::disassemble(const PayloadBuffer& message) {
    return msgpack_unpack_to<Dictionary>(to_char_ptr(message.span().data()), message.span().size());
}

// --- Composite ---

std::string Composite::to_string() const {
    return std::visit([](const auto& arg) { return arg.to_string(); }, *this);
}

std::string Composite::demangle() const {
    return std::visit([](const auto& arg) { return arg.demangle(); }, *this);
}

void Composite::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    // All types of the variant support packing
    std::visit([&](const auto& arg) { msgpack_packer.pack(arg); }, *this);
}

void Composite::msgpack_unpack(const msgpack::object& msgpack_object) {
    switch(msgpack_object.type) {
    case msgpack::type::ARRAY: {
        emplace<Array>(msgpack_object.as<Array>());
        break;
    }
    case msgpack::type::MAP: {
        emplace<Dictionary>(msgpack_object.as<Dictionary>());
        break;
    }
    default: {
        // If not array or map, try to unpack as scalar
        emplace<Scalar>(msgpack_object.as<Scalar>());
    }
    }
}

PayloadBuffer Composite::assemble() const {
    msgpack::sbuffer sbuf {};
    ::msgpack_pack(sbuf, *this);
    return {std::move(sbuf)};
}

Composite Composite::disassemble(const PayloadBuffer& message) {
    return msgpack_unpack_to<Composite>(to_char_ptr(message.span().data()), message.span().size());
}

// --- Composite List ---

std::string CompositeList::to_string() const {
    std::string out = "[";
    if(!empty()) {
        out += " " + range_to_string(*this, [](const auto& e) { return e.to_string(); }) + " ";
    }
    out += "]";
    return out;
}

void CompositeList::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    msgpack_packer.pack_array(size());
    for(const auto& element : *this) {
        msgpack_packer.pack(element);
    }
}

void CompositeList::msgpack_unpack(const msgpack::object& msgpack_object) {
    // Unpack array
    if(msgpack_object.type != msgpack::type::ARRAY) [[unlikely]] {
        throw msgpack::type_error();
    }
    const auto msgpack_array_raw = msgpack_object.via.array; // NOLINT(cppcoreguidelines-pro-type-union-access)
    const auto msgpack_array = std::span(msgpack_array_raw.ptr, msgpack_array_raw.size);

    // Unpack values into the vector
    reserve(msgpack_array_raw.size);
    for(const auto& msgpack_val : msgpack_array) {
        emplace_back(msgpack_val.as<Composite>());
    }
}

PayloadBuffer CompositeList::assemble() const {
    msgpack::sbuffer sbuf {};
    ::msgpack_pack(sbuf, *this);
    return {std::move(sbuf)};
}

CompositeList CompositeList::disassemble(const PayloadBuffer& message) {
    return msgpack_unpack_to<CompositeList>(to_char_ptr(message.span().data()), message.span().size());
}

// NOLINTEND(misc-no-recursion)
