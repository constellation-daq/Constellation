/**
 * @file
 * @brief Dictionary type with serialization functions for MessagePack
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <variant>

#include <msgpack/object_decl.hpp>
#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/core/config.hpp"

namespace constellation::config {

    /**
     * Value type for Dictionary using std::variant
     *
     * Allowed types: nil, bool, int, float, string and time point
     */
    using DictionaryValue = std::variant<std::monostate,
                                         bool,
                                         std::int64_t,
                                         double,
                                         std::string,
                                         std::chrono::system_clock::time_point,
                                         std::vector<std::int64_t>,
                                         std::vector<double>,
                                         std::vector<std::string>>;

    template <typename... Ts> struct Overload : Ts... {
        using Ts::operator()...;
    };
    template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

    template <class V> std::string get_string(V const& v) {
        std::ostringstream out {};

        auto visitor = Overload {
            [&](const std::monostate&) { out << "NULL"; },
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
        };

        std::visit(visitor, v);
        return out.str();
    }

    template <class V> std::type_info const& get_type(V const& v) {
        return std::visit([](auto&& x) -> decltype(auto) { return typeid(x); }, v);
    }

    /**
     * Dictionary type with serialization functions for MessagePack
     */
    class Dictionary final : public std::map<std::string, DictionaryValue> {
    public:
        /** Pack dictionary with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack dictionary with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);
    };

} // namespace constellation::config
