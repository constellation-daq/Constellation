/**
 * @file
 * @brief Controller & ConfigParser exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::controller {
    /**
     * @ingroup Exceptions
     * @brief Base class for all controller exceptions
     */
    class CNSTLN_API ControllerError : public utils::RuntimeError {
    public:
        explicit ControllerError(std::string what_arg) : RuntimeError(std::move(what_arg)) {}

    protected:
        ControllerError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Notifies of a missing configuration file
     */
    class CNSTLN_API ConfigFileNotFoundError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration that is not found
         * @param file_name Name of the configuration file
         */
        explicit ConfigFileNotFoundError(const std::filesystem::path& file_name) {
            error_message_ = "Could not read configuration file " + file_name.string();
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error with parsing the configuration
     */
    class CNSTLN_API ConfigParseError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration file that cannot correctly be parsed
         * @param error Error message
         */
        explicit ConfigParseError(std::string_view error) {
            error_message_ = "Could not parse content of configuration: ";
            error_message_ += error;
        }

    protected:
        ConfigParseError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Error while parsing a key
     */
    class CNSTLN_API ConfigKeyError : public ConfigParseError {
    public:
        /**
         * @brief Construct an error for a configuration key
         * @param key Name of the problematic key
         * @param error Error message
         */
        explicit ConfigKeyError(std::string_view key, std::string_view error) {
            error_message_ = "Error while parsing key " + utils::quote(key) + " in configuration: ";
            error_message_ += error;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error while parsing a value
     */
    class CNSTLN_API ConfigValueError : public ConfigParseError {
    public:
        /**
         * @brief Construct an error for a configuration value
         * @param key Name of the problematic key
         * @param error Error message
         */
        explicit ConfigValueError(std::string_view key, std::string_view error) {
            error_message_ = "Error while parsing value of key " + utils::quote(key) + " in configuration: ";
            error_message_ += error;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error in configuration validation
     */
    class CNSTLN_API ConfigValidationError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration validation failure
         * @param error Error message
         */
        explicit ConfigValidationError(std::string_view error) {
            error_message_ = "Error validating configuration: ";
            error_message_ += error;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error from a measurement queue
     */
    class CNSTLN_API QueueError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a measurement queue
         * @param error Error message from the queue
         */
        explicit QueueError(std::string_view error) {
            error_message_ = "Measurement queue error: ";
            error_message_ += error;
        }
    };
} // namespace constellation::controller
