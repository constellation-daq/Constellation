/**
 * @file
 * @brief Implementation of Dictionary
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Dictionary.hpp"

#include <msgpack.hpp>

#include <span>
#include <type_traits>
#include <variant>

using namespace constellation::config;

void Dictionary::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    msgpack_packer.pack_map(this->size());
    for(auto const& [key, val] : *this) {
        msgpack_packer.pack(key);
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr(std::is_same_v<T, std::monostate>) {
                    // Monostate => nil
                    msgpack_packer.pack_nil();
                } else {
                    // Other types have implementation
                    msgpack_packer.pack(arg);
                }
            },
            val);
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
        DictionaryValue value {};
        switch(msgpack_kv.val.type) {
        case msgpack::type::BOOLEAN: {
            value = msgpack_kv.val.as<bool>();
            break;
        }
        case msgpack::type::POSITIVE_INTEGER:
        case msgpack::type::NEGATIVE_INTEGER: {
            value = msgpack_kv.val.as<std::int64_t>();
            break;
        }
        case msgpack::type::FLOAT32:
        case msgpack::type::FLOAT64: {
            value = msgpack_kv.val.as<double>();
            break;
        }
        case msgpack::type::STR: {
            value = msgpack_kv.val.as<std::string>();
            break;
        }
        case msgpack::type::EXT: {
            // Try to convert to time_point, throws if wrong EXT type
            value = msgpack_kv.val.as<std::chrono::system_clock::time_point>();
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

        // Insert / overwrite in the map
        this->insert_or_assign(key, std::move(value));
    }
}
