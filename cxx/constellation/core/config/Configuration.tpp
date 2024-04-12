/**
 * @file
 * @brief Template implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "Configuration.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <magic_enum.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::config {

    template <typename T> T Configuration::get(const std::string& key) const {
        try {
            const auto& dictval = config_.at(key);
            auto val = dictval.get<T>();
            used_keys_.markUsed(key);
            return val;
        } catch(std::out_of_range& e) {
            /* Requested key has not been found in dictionary */
            throw MissingKeyError(key);
        } catch(std::bad_variant_access& e) {
            /* Value held by the dictionary entry could not be cast to desired type */
            throw InvalidTypeError(key, config_.at(key).type(), typeid(T));
        } catch(std::invalid_argument& e) {
            /* Value held by the dictionary entry could not be converted to desired type */
            throw InvalidValueError(config_.at(key).str(), key, e.what());
        }
    }

    template <typename T> T Configuration::get(const std::string& key, const T& def) const {
        if(has(key)) {
            return get<T>(key);
        }
        return def;
    }

    template <typename T> std::vector<T> Configuration::getArray(const std::string& key, const std::vector<T>& def) const {
        if(has(key)) {
            return getArray<T>(key);
        }
        return def;
    }

    template <typename T> void Configuration::set(const std::string& key, const T& val, bool mark_used) {
        try {
            config_[key] = Value::set(val);
            used_keys_.registerMarker(key);
            if(mark_used) {
                used_keys_.markUsed(key);
            }
        } catch(std::bad_cast& e) {
            /* Value held by the dictionary entry could not be cast to desired type */
            throw InvalidTypeError(key, typeid(T), typeid(value_t));
        } catch(std::overflow_error& e) {
            // FIXME to_string utils need to be extended for us!
            throw InvalidValueError("std::to_string(val)", key, e.what());
        }
    }

    template <typename T> void Configuration::setDefault(const std::string& key, const T& val) {
        if(!has(key)) {
            set<T>(key, val, true);
        }
    }

    template <typename T> void Configuration::setDefaultArray(const std::string& key, const std::vector<T>& val) {
        if(!has(key)) {
            setArray<T>(key, val, true);
        }
    }
} // namespace constellation::config
