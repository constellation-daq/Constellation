/**
 * @file
 * @brief Command dispatcher for user commands
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

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

#include "constellation/core/config.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/casts.hpp"
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
    class CNSTLN_API CommandRegistry {
    public:
        /**
         * @brief Register a command with arbitrary arguments from a functional
         *
         * @param name Name of the command
         * @param description Description of the command
         * @param states States of the finite state machine in which this command can be called
         * @param func Functional containing the callable object
         * @tparam R Return type
         * @tparam Args Argument types
         */
        template <typename R, typename... Args>
        void add(const std::string& name,
                 std::string description,
                 std::initializer_list<constellation::message::State> states,
                 std::function<R(Args...)> func);

        /**
         * @brief Register a command with arbitrary arguments from a member function pointer and object pointer
         *
         * @param name Name of the command
         * @param description Description of the command
         * @param states States of the finite state machine in which this command can be called
         * @param func Pointer to the member function of t to be called
         * @param t Pointer to the called object
         * @tparam T Type of the called object
         * @tparam R Return type
         * @tparam Args Argument types
         */
        template <typename T, typename R, typename... Args>
        void add(std::string name,
                 std::string description,
                 std::initializer_list<constellation::message::State> states,
                 R (T::*func)(Args...),
                 T* t);

        /**
         * @brief Calls a registered function with its arguments
         * This method calls a registered function and returns the output of the function, or an empty string.
         *
         * @param state Current state of the finite state machine when this call was made
         * @param name Name of the command to be called
         * @param args Vector with arguments encoded as std::string
         * @return Return value of the called function encoded as std::string
         *
         * @throws UnknownUserCommand if no command is not registered under this name
         * @throws InvalidUserCommand if the command is registered but cannot be called in the current state
         * @throws MissingUserCommandArguments if the number of arguments does not match
         * @throws std::invalid_argument if an argument or the return value could not be decoded or encoded to std::string
         */
        config::Value call(message::State state, const std::string& name, const config::List& args);

        /**
         * @brief Generate map of commands with comprehensive description
         *
         * The description consists of the user-provided command description from registering the command. In addition, this
         * description is appended with a statement on how many arguments the command requires and a list of states in which
         * the command can be called.
         *
         * @return Map with command names and descriptions
         */
        std::map<std::string, std::string> describeCommands() const;

    private:
        using Call = std::function<config::Value(const config::List&)>;

        /**
         * @struct Command
         * @brief Struct holding all information for a command
         * Struct holding the command function call, its number of required arguments, the description and valid
         * states of the finite state machine it can be called for.
         */
        struct Command {
            Call func;
            std::size_t nargs;
            std::string description;
            std::set<constellation::message::State> valid_states;
        };

        // Map of registered commands
        std::unordered_map<std::string, Command> commands_;

        template <typename T> static inline T value(const config::Value& value);
        template <typename T> static inline config::Value convert(const T& value);

        // Wrapper for command with return value
        template <typename R, typename... Args> struct Wrapper {
            std::function<R(Args...)> func;

            config::Value operator()(const config::List& args) {
                return call_command(args, std::index_sequence_for<Args...> {});
            }
            template <std::size_t... I>
            config::Value call_command(const config::List& args, std::index_sequence<I...> /*unused*/) {
                return convert(func(value<typename std::decay_t<Args>>(args.at(I))...));
            }
        };

        // Wrapper for command without return value
        template <typename... Args> struct Wrapper<void, Args...> {
            std::function<void(Args...)> func;

            config::Value operator()(const config::List& args) {
                return call_command(args, std::index_sequence_for<Args...> {});
            }
            template <std::size_t... I>
            config::Value call_command(const config::List& args, std::index_sequence<I...> /*unused*/) {
                func(value<typename std::decay_t<Args>>(args.at(I))...);
                return {};
            }
        };

        /**
         * @brief Generator method for Call objects
         *
         * @param function Function to be called
         * @tparam R Return type
         * @tparam Args Argument types
         * @return Call object
         */
        template <typename R, typename... Args>
        inline CommandRegistry::Call generate_call(std::function<R(Args...)>&& function) {
            return Wrapper<R, Args...> {std::move(function)};
        }
    };

    template <typename T> inline T CommandRegistry::value(const config::Value& value) {
        try {
            return std::get<T>(value);
        } catch(std::bad_variant_access&) {
            std::string msg;
            msg += "Mismatch of argument type \"";
            msg += utils::demangle(typeid(T).name());
            msg += "\" to provided type \"";
            msg += utils::demangle(value.type().name());
            msg += "\"";
            throw std::invalid_argument(msg);
        }
    }

    template <typename T> inline config::Value CommandRegistry::convert(const T& value) {
        try {
            return config::Value(value);
        } catch(std::bad_variant_access&) {
            std::string msg;
            msg += "Error casting provided type \"";
            msg += utils::demangle(typeid(T).name());
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
