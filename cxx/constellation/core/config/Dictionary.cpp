/**
 * @file
 * @brief Implementation of Dictionary
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Dictionary.hpp"
#include "constellation/core/utils/std23.hpp"

#include <msgpack.hpp>

#include <span>
#include <type_traits>
#include <variant>

using namespace constellation::config;

std::string DictionaryValue::str() const {
    std::ostringstream out {};

    std::visit(overload {
                   [&](const std::monostate&) { out << "NULL"; },
                   [&](const bool& arg) { out << std::boolalpha << arg; },
                   [&](const std::vector<double>& arg) {
                       out << "[";
                       for(const auto& val : arg) {
                           out << val << ", ";
                       }
                       out << "]";
                   },
                   [&](const std::vector<std::int64_t>& arg) {
                       out << "[";
                       for(const auto& val : arg) {
                           out << val << ", ";
                       }
                       out << "]";
                   },
                   [&](const std::vector<std::string>& arg) {
                       out << "[";
                       for(const auto& val : arg) {
                           out << val << ", ";
                       }
                       out << "]";
                   },
                   [&](const auto& arg) { out << arg; },
               },
               *this);
    return out.str();
}

std::type_info const& DictionaryValue::type() const {
    return std::visit([](auto&& x) -> decltype(auto) { return typeid(x); }, *this);
}

void DictionaryValue::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    auto visitor = overload {
        [&](const std::monostate&) { msgpack_packer.pack(msgpack::type::nil_t()); },
        [&](const auto& arg) { msgpack_packer.pack(arg); },
    };
    std::visit(visitor, *this);
}

DictionaryValue DictionaryValue::msgpack_unpack(const msgpack::object& msgpack_object) {
    DictionaryValue value {};
    switch(msgpack_object.type) {
    case msgpack::type::BOOLEAN: {
        value = msgpack_object.as<bool>();
        break;
    }
    case msgpack::type::POSITIVE_INTEGER:
    case msgpack::type::NEGATIVE_INTEGER: {
        value = msgpack_object.as<std::int64_t>();
        break;
    }
    case msgpack::type::FLOAT32:
    case msgpack::type::FLOAT64: {
        value = msgpack_object.as<double>();
        break;
    }
    case msgpack::type::STR: {
        value = msgpack_object.as<std::string>();
        break;
    }
    case msgpack::type::EXT: {
        // Try to convert to time_point, throws if wrong EXT type
        value = msgpack_object.as<std::chrono::system_clock::time_point>();
        break;
    }
    case msgpack::type::NIL: {
        value = std::monostate();
        break;
    }
    default: {
        throw msgpack::type_error();
    }
    }
    return value;
}

void Dictionary::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    msgpack_packer.pack_map(this->size());

    for(auto const& [key, val] : *this) {
        msgpack_packer.pack(key);
        val.msgpack_pack(msgpack_packer);
    }
}

void Dictionary::msgpack_unpack(const msgpack::object& msgpack_object) {
    // Unpack map
    if(msgpack_object.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    const auto msgpack_map_raw = msgpack_object.via.map; // NOLINT(cppcoreguidelines-pro-type-union-access)
    const auto msgpack_map = std::span(msgpack_map_raw.ptr, msgpack_map_raw.size);

    for(const auto& msgpack_kv : msgpack_map) {
        // Unpack key
        if(msgpack_kv.key.type != msgpack::type::STR) {
            throw msgpack::type_error();
        }
        const auto key = msgpack_kv.key.as<std::string>();

        // Unpack value
        const auto value = DictionaryValue::msgpack_unpack(msgpack_kv.val);

        // Insert / overwrite in the map
        this->insert_or_assign(key, std::move(value));
    }
}
