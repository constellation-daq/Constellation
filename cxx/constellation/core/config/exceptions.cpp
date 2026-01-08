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
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::utils;

namespace {
    std::string get_prefixed_key(const Section& config, std::string_view key) {
        auto prefixed_key = std::string(config.prefix());
        prefixed_key += key;
        return quote(prefixed_key);
    }
} // namespace

MissingKeyError::MissingKeyError(const Section& config, std::string_view key) {
    error_message_ = "Key " + get_prefixed_key(config, key) + " does not exist";
}

InvalidKeyError::InvalidKeyError(const Section& config, std::string_view key, std::string_view reason) {
    error_message_ = "Key " + get_prefixed_key(config, key) + " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidTypeError::InvalidTypeError(const Section& config,
                                   std::string_view key,
                                   std::string_view vtype,
                                   std::string_view type) {
    error_message_ = "Could not convert value of type " + quote(vtype) + " to type " + quote(type) + " for key " +
                     get_prefixed_key(config, key);
}

InvalidValueError::InvalidValueError(const Section& config, std::string_view key, std::string_view reason) {
    error_message_ = "Value of key " + get_prefixed_key(config, key) + " is not valid: ";
    error_message_ += reason;
}

InvalidCombinationError::InvalidCombinationError(const Section& config,
                                                 std::initializer_list<std::string> keys,
                                                 std::string_view reason) {
    std::vector<std::string> present_keys {};
    for(const auto& key : keys) {
        if(!config.has(key)) {
            continue;
        }
        present_keys.emplace_back(get_prefixed_key(config, key));
    }
    error_message_ = "Combination of keys " + range_to_string(present_keys) + " is not valid";
    if(!reason.empty()) {
        error_message_ += ": ";
        error_message_ += reason;
    }
}

InvalidUpdateError::InvalidUpdateError(const Section& config, std::string_view key, std::string_view reason) {
    error_message_ = "Failed to update value of key " + get_prefixed_key(config, key) + ": ";
    error_message_ += reason;
}
