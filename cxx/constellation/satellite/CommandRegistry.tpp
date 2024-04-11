/**
 * @file
 * @brief Command dispatcher for user commands
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "CommandRegistry.hpp"

#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "exceptions.hpp"

namespace constellation::satellite {

    template <typename T> inline T CommandRegistry::to_argument(const config::Value& value) {
        try {
            return value.get<T>();
        } catch(std::bad_variant_access&) {
            std::string msg;
            msg += "Mismatch of argument type \"";
            msg += utils::demangle(typeid(T));
            msg += "\" to provided type \"";
            msg += utils::demangle(value.type());
            msg += "\"";
            throw std::invalid_argument(msg);
        }
    }

    template <typename T> inline config::Value CommandRegistry::convert(const T& value) {
        try {
            return config::Value::set(value);
        } catch(std::bad_cast&) {
            std::string msg;
            msg += "Error casting function return type \"";
            msg += utils::demangle(typeid(T));
            msg += "\" to dictionary value";
            throw std::invalid_argument(msg);
        }
    }

    template <typename R, typename... Args>
    inline void CommandRegistry::add(const std::string& name,
                                     std::string description,
                                     std::initializer_list<constellation::message::State> states,
                                     std::function<R(Args...)> func) {
        if(name.empty()) {
            throw utils::LogicError("Can not register command with empty name");
        }

        const auto [it, success] = commands_.emplace(
            name, Command {generate_call(std::move(func)), sizeof...(Args), std::move(description), states});

        if(!success) {
            throw utils::LogicError("Command \"" + name + "\" is already registered");
        }
    }

    template <typename T, typename R, typename... Args>
    inline void CommandRegistry::add(std::string name,
                                     std::string description,
                                     std::initializer_list<constellation::message::State> states,
                                     R (T::*func)(Args...),
                                     T* t) {
        if(!func || !t) {
            throw utils::LogicError("Object and member function pointers must not be nullptr");
        }
        add(std::move(name), std::move(description), states, std::function<R(Args...)>([=](Args... args) {
                return (t->*func)(args...);
            }));
    }

} // namespace constellation::satellite
