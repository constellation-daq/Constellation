/**
 * @file
 * @brief Implementation of configuration parser
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ControllerConfig.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include <toml++/toml.hpp>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::controller;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::utils;

ControllerConfiguration::ControllerConfiguration(const std::filesystem::path& path) {
    // Check if file exists
    if(!std::filesystem::is_regular_file(path)) {
        throw ConfigFileNotFoundError(path);
    }

    // Convert to absolute path
    const auto file_path_abs = std::filesystem::canonical(path);
    LOG(config_parser_logger_, DEBUG) << "Parsing configuration file " << std::quoted(file_path_abs.string());

    const std::ifstream file {file_path_abs};
    std::ostringstream buffer {};
    buffer << file.rdbuf();

    parse_toml(buffer.view());
}

ControllerConfiguration::ControllerConfiguration(std::string_view toml) {
    parse_toml(toml);
}

void ControllerConfiguration::parse_toml(std::string_view toml) {
    toml::table tbl {};
    try {
        tbl = toml::parse(toml);
    } catch(const toml::parse_error& err) {
        std::ostringstream oss {};
        oss << err;
        throw ConfigFileParseError(oss.view());
    }

    auto parse_value = [&](const toml::key& key, auto&& val) -> std::optional<Value> {
        LOG(config_parser_logger_, TRACE) << "Reading key " << key;
        if constexpr(toml::is_table<decltype(val)>) {
            LOG(config_parser_logger_, TRACE) << "Skipping table for key " << key;
            return {};
        }
        if constexpr(toml::is_array<decltype(val)>) {
            const auto& arr = val.as_array();
            // Throw if non-empty inhomogeneous array (empty arrays are never homogeneous)
            if(!val.is_homogeneous() && !arr->empty()) {
                throw ConfigFileTypeError(key.str(), "Array is not homogeneous");
            }
            LOG(config_parser_logger_, TRACE) << "Found homogeneous array for key " << key;
            if(arr->empty()) {
                return std::monostate();
            }
            if(arr->front().is_integer()) {
                std::vector<std::int64_t> return_value {};
                return_value.reserve(arr->size());
                for(auto&& elem : *arr) {
                    return_value.push_back(elem.as_integer()->get());
                }
                return return_value;
            }
            if(arr->front().is_floating_point()) {
                std::vector<double> return_value {};
                return_value.reserve(arr->size());
                for(auto&& elem : *arr) {
                    return_value.push_back(elem.as_floating_point()->get());
                }
                return return_value;
            }
            if(arr->front().is_boolean()) {
                std::vector<bool> return_value {};
                return_value.reserve(arr->size());
                for(auto&& elem : *arr) {
                    return_value.push_back(elem.as_boolean()->get());
                }
                return return_value;
            }
            if(arr->front().is_string()) {
                std::vector<std::string> return_value {};
                return_value.reserve(arr->size());
                for(auto&& elem : *arr) {
                    return_value.push_back(elem.as_string()->get());
                }
                return return_value;
            }
            // If not returned yet then unknown type
            throw ConfigFileTypeError(key.str(), "Unknown type");
        }
        if constexpr(toml::is_integer<decltype(val)>) {
            return val.as_integer()->get();
        }
        if constexpr(toml::is_floating_point<decltype(val)>) {
            return val.as_floating_point()->get();
        }
        if constexpr(toml::is_boolean<decltype(val)>) {
            return val.as_boolean()->get();
        }
        if constexpr(toml::is_string<decltype(val)>) {
            return val.as_string()->get();
        }
        // If not returned yet then unknown type
        throw ConfigFileTypeError(key.str(), "Unknown type");
    };

    // Find satellites base node
    if(const auto& node = tbl.at_path("satellites")) {

        // Loop over all nodes:
        node.as_table()->for_each([&](const toml::key& global_key, auto&& global_val) {
            // Check if this is a table and represents a satellite type:
            if constexpr(toml::is_table<decltype(global_val)>) {
                LOG(config_parser_logger_, DEBUG) << "Found satellite type sub-node " << global_key;
                auto type_lc = transform(global_key.str(), ::tolower);
                Dictionary dict_type {};

                global_val.as_table()->for_each([&](const toml::key& type_key, auto&& type_val) {
                    // Check if this is a table and represents an individual satellite
                    if constexpr(toml::is_table<decltype(type_val)>) {
                        LOG(config_parser_logger_, DEBUG) << "Found satellite name sub-node " << type_key;
                        auto canonical_name_lc = type_lc + "." + transform(type_key.str(), ::tolower);
                        Dictionary dict_name {};

                        type_val.as_table()->for_each([&](const toml::key& name_key, auto&& name_val) {
                            const auto value = parse_value(name_key, name_val);
                            if(value.has_value()) {
                                dict_name.emplace(transform(name_key, ::tolower), value.value());
                            }
                        });

                        // Add satellite dictionary
                        satellite_configs_.emplace(std::move(canonical_name_lc), std::move(dict_name));

                    } else {
                        const auto value = parse_value(type_key, type_val);
                        if(value.has_value()) {
                            dict_type.emplace(transform(type_key.str(), ::tolower), value.value());
                        }
                    }
                });

                // Add type dictionary
                type_configs_.emplace(std::move(type_lc), std::move(dict_type));
            } else {
                const auto value = parse_value(global_key, global_val);
                if(value.has_value()) {
                    global_config_.emplace(transform(global_key.str(), ::tolower), value.value());
                }
            }
        });

    } else {
        LOG(config_parser_logger_, WARNING) << "Could not find base node for satellites";
    }
}

Dictionary ControllerConfiguration::getSatelliteConfiguration(std::string_view canonical_name) const {

    Dictionary config;

    const auto separator = canonical_name.find_first_of('.');
    const auto type = canonical_name.substr(0, separator);

    // We need to look for canonical name - and case-insensitive
    const auto cfg_it = std::ranges::find_if(satellite_configs_, [canonical_name](const auto& r) {
        return transform(r.first, ::tolower) == transform(canonical_name, ::tolower);
    });
    if(cfg_it != satellite_configs_.end()) {
        config = cfg_it->second;
    }

    // Add type keys
    const auto type_it = std::ranges::find_if(
        type_configs_, [type](const auto& r) { return transform(r.first, ::tolower) == transform(type, ::tolower); });
    if(type_it != type_configs_.end()) {
        for(const auto& [key, value] : type_it->second) {
            const auto& [it, inserted] = config.insert({key, value});
            LOG_IF(config_parser_logger_, DEBUG, inserted) << "Added key " << key << " from type section";
        }
    }

    for(const auto& [key, value] : global_config_) {
        const auto& [it, inserted] = config.insert({key, value});
        LOG_IF(config_parser_logger_, DEBUG, inserted) << "Added key " << key << " from global satellites section";
    }

    return config;
}
