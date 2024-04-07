/**
 * @file
 * @brief Implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Configuration.hpp"

#include <cassert>
#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <string>

#include <msgpack.hpp>

#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation::config;

Configuration::Configuration(const Dictionary& dict) : config_(dict) {
    // Register all markers:
    for(const auto& [key, val] : dict) {
        used_keys_.registerMarker(key);
    }
};

Configuration::AccessMarker::AccessMarker(const Configuration::AccessMarker& rhs) {
    for(const auto& [key, value] : rhs.markers_) {
        registerMarker(key);
        markers_.at(key).store(value.load());
    }
}

Configuration::AccessMarker& Configuration::AccessMarker::operator=(const Configuration::AccessMarker& rhs) {
    if(this == &rhs) {
        return *this;
    }

    for(const auto& [key, value] : rhs.markers_) {
        registerMarker(key);
        markers_.at(key).store(value.load());
    }
    return *this;
}

void Configuration::AccessMarker::registerMarker(const std::string& key) {
    markers_.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
}

bool Configuration::has(const std::string& key) const {
    return config_.find(key) != config_.cend();
}

unsigned int Configuration::count(std::initializer_list<std::string> keys) const {
    if(keys.size() == 0) {
        throw std::invalid_argument("list of keys cannot be empty");
    }

    unsigned int found = 0;
    for(const auto& key : keys) {
        if(has(key)) {
            found++;
        }
    }
    return found;
}

/**
 * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
 *
 * For a relative path the absolute path of the configuration file is prepended. Absolute paths are not changed.
 */
std::filesystem::path Configuration::getPath(const std::string& key, bool check_exists) const {
    try {
        return path_to_absolute(get<std::string>(key), check_exists);
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(config_.at(key).str(), key, e.what());
    }
}
/**
 * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
 *
 * For a relative path the absolute path of the configuration file is prepended. Absolute paths are not changed.
 */
std::filesystem::path Configuration::getPathWithExtension(const std::string& key,
                                                          const std::string& extension,
                                                          bool check_exists) const {
    try {
        return path_to_absolute(std::filesystem::path(get<std::string>(key)).replace_extension(extension), check_exists);
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(config_.at(key).str(), key, e.what());
    }
}
/**
 * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
 *
 * For all relative paths the absolute path of the configuration file is prepended. Absolute paths are not changed.
 */
std::vector<std::filesystem::path> Configuration::getPathArray(const std::string& key, bool check_exists) const {
    std::vector<std::filesystem::path> path_array;

    // Convert all paths to absolute
    try {
        for(auto& path : getArray<std::string>(key)) {
            path_array.emplace_back(path_to_absolute(path, check_exists));
        }
        return path_array;
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(config_.at(key).str(), key, e.what());
    }
}
/**
 * @throws std::invalid_argument If the path does not exists
 */
std::filesystem::path Configuration::path_to_absolute(std::filesystem::path path, bool canonicalize_path) {
    // If not a absolute path, make it an absolute path
    if(!path.is_absolute()) {
        // Get current directory and append the relative path
        path = std::filesystem::current_path() / path;
    }

    // Normalize path only if we have to check if it exists
    // NOTE: This throws an error if the path does not exist
    if(canonicalize_path) {
        try {
            path = std::filesystem::canonical(path);
        } catch(std::filesystem::filesystem_error&) {
            throw std::invalid_argument("path " + path.string() + " not found");
        }
    }
    return path;
}

/**
 *  The alias is only used if new key does not exist but old key does. The old key is automatically marked as used.
 */
void Configuration::setAlias(const std::string& new_key, const std::string& old_key, bool warn) {
    if(!has(old_key) || has(new_key)) {
        return;
    }
    try {
        config_[new_key] = config_.at(old_key);
        used_keys_.registerMarker(new_key);
        used_keys_.markUsed(old_key);
    } catch(std::out_of_range& e) {
        throw MissingKeyError(old_key);
    }

    if(warn) {
        // FIXME logging
        // LOG(WARNING) << "Parameter \"" << old_key << "\" is deprecated and superseded by \"" << new_key << "\"";
    }
}

std::string Configuration::getText(const std::string& key) const {
    try {
        used_keys_.markUsed(key);
        return config_.at(key).str();
    } catch(std::out_of_range& e) {
        throw MissingKeyError(key);
    }
}
std::string Configuration::getText(const std::string& key, const std::string& def) const {
    if(!has(key)) {
        return def;
    }
    return getText(key);
}

/**
 * All keys that are already defined earlier in this configuration are not changed.
 */
void Configuration::merge(const Configuration& other) {
    for(const auto& [key, value] : other.config_) {
        // Only merge values that do not yet exist
        if(!has(key)) {
            set(key, value);
        }
    }
}

Dictionary Configuration::getAll() const {
    Dictionary result;

    // Loop over all configuration keys
    for(const auto& key_value : config_) {
        // Skip internal keys starting with an underscore
        if(!key_value.first.empty() && key_value.first.front() == '_') {
            continue;
        }

        result.emplace(key_value);
    }

    return result;
}

std::vector<std::string> Configuration::getUnusedKeys() const {
    std::vector<std::string> result;

    // Loop over all configuration keys, excluding internal ones
    for(const auto& key_value : getAll()) {
        // Add those to result that have not been accessed:
        if(!used_keys_.isUsed(key_value.first)) {
            result.emplace_back(key_value.first);
        }
    }

    return result;
}
