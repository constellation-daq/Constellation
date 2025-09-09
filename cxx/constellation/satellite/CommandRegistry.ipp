/**
 * @file
 * @brief Command dispatcher for user commands
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "CommandRegistry.hpp" // NOLINT(misc-header-include-cycle)

#include <set>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <variant>

#include "constellation/core/config/Value.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"
#include "constellation/satellite/exceptions.hpp"

namespace constellation::satellite {

    template <typename T> inline T CommandRegistry::to_argument(const config::Value& value) {
        try {
            return value.get<T>();
        } catch(const std::bad_variant_access&) {
            throw InvalidUserCommandArguments(utils::demangle<T>(), value.demangle());
        }
    }

    template <typename T> inline config::Value CommandRegistry::convert(const T& value) {
        try {
            return config::Value::set(value);
        } catch(const std::bad_cast&) {
            throw InvalidUserCommandResult(utils::demangle<T>());
        }
    }

    template <typename C>
        requires utils::is_function_v<C>
    inline void CommandRegistry::add(std::string_view name,
                                     std::string description,
                                     std::set<protocol::CSCP::State> allowed_states,
                                     C function) {
        const auto name_lc = utils::transform(name, ::tolower);
        if(!protocol::CSCP::is_valid_command_name(name_lc)) {
            throw utils::LogicError("Command name " + utils::quote(name_lc) + " is invalid");
        }

        if(utils::enum_cast<protocol::CSCP::StandardCommand>(name_lc).has_value()) {
            throw utils::LogicError("Standard satellite command with this name exists");
        }

        if(utils::enum_cast<protocol::CSCP::TransitionCommand>(name_lc).has_value()) {
            throw utils::LogicError("Satellite transition command with this name exists");
        }

        // Wrap object into a std::function
        using function_traits = utils::function_traits<C>;
        using function_type = typename function_traits::function_type;
        const auto nargs = function_traits::argument_size::value;
        auto call = Call(Wrapper(function_type(std::move(function))));

        const auto [it, success] =
            commands_.emplace(name_lc, Command(std::move(call), nargs, std::move(description), std::move(allowed_states)));

        if(!success) {
            throw utils::LogicError("Command " + utils::quote(name_lc) + " is already registered");
        }
    }

} // namespace constellation::satellite
