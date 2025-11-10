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
using namespace constellation::utils;

MissingKeyError::MissingKeyError(std::string_view key) {
    error_message_ = "Key " + quote(key) + " does not exist";
}

InvalidKeyError::InvalidKeyError(std::string_view key, std::string_view reason) {
    error_message_ = "Key " + quote(key) + " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidTypeError::InvalidTypeError(std::string_view key, std::string_view vtype, std::string_view type) {
    error_message_ =
        "Could not convert value of type " + quote(vtype) + " to type " + quote(type) + " for key " + quote(key);
}

InvalidValueError::InvalidValueError(std::string_view key, std::string_view reason) {
    error_message_ = "Value of key " + quote(key) + " is not valid: ";
    error_message_ += reason;
}

InvalidCombinationError::InvalidCombinationError(const Configuration& config,
                                                 std::initializer_list<std::string> keys,
                                                 std::string_view reason) {
    error_message_ = "Combination of keys ";
    for(const auto& key : keys) {
        if(!config.has(key)) {
            continue;
        }
        error_message_ += quote(key) + ", ";
    }
    error_message_.resize(error_message_.size() - 2);
    error_message_ += " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidUpdateError::InvalidUpdateError(std::string_view key, std::string_view reason) {
    error_message_ = "Failed to update value of key " + quote(key) + ": ";
    error_message_ += reason;
}
