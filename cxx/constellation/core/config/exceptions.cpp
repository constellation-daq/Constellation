/**
 * @file
 * @brief Implementation of configuration exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/Configuration.hpp"

using namespace constellation::config;

InvalidValueError::InvalidValueError(const Configuration&, const std::string& key, const std::string& reason) {
    // FIXME print value: " + config.getText(key) + "
    error_message_ = "Value  of key '" + key + "' is not valid";
    if(!reason.empty()) {
        error_message_ += ": " + reason;
    }
}

InvalidCombinationError::InvalidCombinationError(const Configuration& config,
                                                 std::initializer_list<std::string> keys,
                                                 const std::string& reason) {
    error_message_ = "Combination of keys ";
    for(const auto& key : keys) {
        if(!config.has(key)) {
            continue;
        }
        error_message_ += "'" + key + "', ";
    }
    error_message_ += " is not valid";
    if(!reason.empty()) {
        error_message_ += ": " + reason;
    }
}
