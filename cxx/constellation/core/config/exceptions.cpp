/**
 * @file
 * @brief Implementation of configuration exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "exceptions.hpp"

#include <initializer_list>
#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;

MissingKeyError::MissingKeyError(std::string_view key) {
    error_message_ = "Key ";
    error_message_ += utils::quote(key);
    error_message_ += " does not exist";
}

InvalidKeyError::InvalidKeyError(std::string_view key, std::string_view reason) {
    error_message_ = "Key ";
    error_message_ += utils::quote(key);
    error_message_ += " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidTypeError::InvalidTypeError(std::string_view key,
                                   std::string_view vtype,
                                   std::string_view type,
                                   std::string_view reason) {
    // FIXME wording
    error_message_ = "Could not convert value of type ";
    error_message_ += utils::quote(vtype);
    error_message_ += " to type ";
    error_message_ += utils::quote(type);
    error_message_ += " for key ";
    error_message_ += utils::quote(key);
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidValueError::InvalidValueError(const Configuration& config, const std::string& key, std::string_view reason)
    : InvalidValueError(config.getText(key), key, reason) {}

InvalidValueError::InvalidValueError(const std::string& value, const std::string& key, std::string_view reason) {
    error_message_ = "Value " + utils::quote(value) + " of key " + utils::quote(key) + " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidCombinationError::InvalidCombinationError(const Configuration& config,
                                                 std::initializer_list<std::string> keys,
                                                 std::string_view reason) {
    error_message_ = "Combination of keys ";
    for(const auto& key : keys) {
        if(!config.has(key)) {
            continue;
        }
        error_message_ += utils::quote(key) + ", ";
    }
    error_message_.resize(error_message_.size() - 2);
    error_message_ += " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}
