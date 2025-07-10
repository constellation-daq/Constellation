/**
 * @file
 * @brief Command dispatcher for user commands
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::satellite {

    /**
     * @class CommandRegistry
     * @brief Registry for user commands
     *
     * Class to allow registration and execution of arbitrary commands based on their name. The commands can require any
     * number of arguments that can be converted from std::string. Return values are also possible as long as a conversion
     * to std::string is possible.
     */
    class CommandRegistry {
    public:
        /**
         * @brief Register a command with arbitrary arguments
         *
         * @param name Name of the command
         * @param description Description of the command
         * @param allowed_states Set of states in which this command can be called
         * @param function Function to add
         */
        template <typename C>
            requires utils::is_function_v<C>
        void add(std::string_view name, std::string description, std::set<protocol::CSCP::State> allowed_states, C function);

        /**
         * @brief Calls a registered function with its arguments
         * This method calls a registered function and returns the output of the function, or an empty string.
         *
         * @param state Current state of the finite state machine when this call was made
         * @param name Name of the command to be called
         * @param args List of arguments
         * @return Return value of the called function
         *
         * @throws UnknownUserCommand if no command is not registered under this name
         * @throws InvalidUserCommand if the command is registered but cannot be called in the current state
         * @throws MissingUserCommandArguments if the number of arguments does not match
         * @throws std::invalid_argument if an argument or the return value could not be decoded or encoded to std::string
         */
        CNSTLN_API config::Value call(protocol::CSCP::State state, const std::string& name, const config::List& args);

        /**
         * @brief Generate map of commands with comprehensive description
         *
         * The description consists of the user-provided command description from registering the command. In addition, this
         * description is appended with a statement on how many arguments the command requires and a list of states in which
         * the command can be called.
         *
         * @return Map with command names and descriptions
         */
        CNSTLN_API std::map<std::string, std::string> describeCommands() const;

    private:
        // Function alias for std::function used in storage
        using Call = std::function<config::Value(const config::List&)>;

        /**
         * @brief Struct holding all information for a command
         *
         * Struct holding the command function call, its number of required arguments, the description and valid
         * states of the finite state machine it can be called for.
         */
        struct Command {
            Call func;
            std::size_t nargs;
            std::string description;
            std::set<protocol::CSCP::State> allowed_states;
        };

        template <typename T> static inline T to_argument(const config::Value& value);
        template <typename T> static inline config::Value convert(const T& value);

        // Wrapper for command extracting function arguments from list
        template <typename R, typename... Args> struct Wrapper {
            std::function<R(Args...)> func;

            config::Value operator()(const config::List& args) {
                return callCommand(args, std::index_sequence_for<Args...> {});
            }

            template <std::size_t... I>
            config::Value callCommand(const config::List& args, std::index_sequence<I...> /*unused*/) {
                if constexpr(std::same_as<R, void>) {
                    func(to_argument<typename std::decay_t<Args>>(args.at(I))...);
                    return {};
                } else {
                    return convert(func(to_argument<typename std::decay_t<Args>>(args.at(I))...));
                }
            }
        };

    private:
        // Map of registered commands
        utils::string_hash_map<Command> commands_;
    };

} // namespace constellation::satellite

// Include template members
#include "CommandRegistry.ipp" // IWYU pragma: keep
