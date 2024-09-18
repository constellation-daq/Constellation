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

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"

#include "exceptions.hpp"

using namespace constellation::controller;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::utils;

ControllerConfiguration::ControllerConfiguration(const std::filesystem::path& path) {
    // Convert main file to absolute path
    const auto file_path_abs = std::filesystem::canonical(path);
    LOG(config_parser_logger_, DEBUG) << "Parsing configuration file " << std::quoted(file_path_abs.string());

    std::ifstream file(file_path_abs);
    if(!file || !std::filesystem::is_regular_file(file_path_abs)) {
        throw ConfigFileNotFoundError(file_path_abs);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    ControllerConfiguration(buffer.str());
}

ControllerConfiguration::ControllerConfiguration(std::string_view toml) {

    toml::table tbl {};
    try {
        tbl = toml::parse(toml);
    } catch(const toml::parse_error& err) {
        std::stringstream s;
        s << err;
        throw ConfigFileParseError(s.str());
    }

    auto parse_value = [&](const toml::key& key, auto&& val) -> std::optional<Value> {
        if constexpr(toml::is_table<decltype(val)>) {
            LOG(config_parser_logger_, DEBUG) << "Skipping table for key " << key;
            return {};
        } else if constexpr(toml::is_array<decltype(val)>) {
            if(val.is_homogeneous()) {
                LOG(config_parser_logger_, DEBUG) << "Found homogeneous array for key " << key;
                const auto& arr = val.as_array();
                if(arr->empty()) {
                    return std::monostate {};
                } else if(arr->front().is_integer()) {
                    std::vector<std::int64_t> return_value {};
                    for(auto&& elem : *arr) {
                        return_value.push_back(elem.as_integer()->get());
                    }
                    return return_value;
                } else if(arr->front().is_floating_point()) {
                    std::vector<double> return_value {};
                    for(auto&& elem : *arr) {
                        return_value.push_back(elem.as_floating_point()->get());
                    }
                    return return_value;
                } else if(arr->front().is_boolean()) {
                    std::vector<bool> return_value {};
                    for(auto&& elem : *arr) {
                        return_value.push_back(elem.as_boolean()->get());
                    }
                    return return_value;
                } else if(arr->front().is_string()) {
                    std::vector<std::string> return_value {};
                    for(auto&& elem : *arr) {
                        return_value.push_back(elem.as_string()->get());
                    }
                    return return_value;
                } else {
                    throw ConfigFileTypeError(key.str(), "Unknown type");
                }
            } else {
                throw ConfigFileTypeError(key.str(), "Array is not homogeneous");
            }
        } else {
            if constexpr(toml::is_integer<decltype(val)>) {
                return val.as_integer()->get();
            } else if constexpr(toml::is_floating_point<decltype(val)>) {
                return val.as_floating_point()->get();
            } else if constexpr(toml::is_boolean<decltype(val)>) {
                return val.as_boolean()->get();
            } else if constexpr(toml::is_string<decltype(val)>) {
                return val.as_string()->get();
            } else {
                throw ConfigFileTypeError(key.str(), "Unknown type");
            }
        }
    };

    // Find satellites base node
    if(const auto& node = tbl.at_path("satellites")) {

        // Loop over all nodes:
        node.as_table()->for_each([&](const toml::key& global_key, auto&& global_val) {
            // Check if this is a table and represents a satellite type:
            if constexpr(toml::is_table<decltype(global_val)>) {
                Dictionary dict_type;

                LOG(config_parser_logger_, DEBUG) << "Found satellite type sub-node " << global_key;
                global_val.as_table()->for_each([&](const toml::key& type_key, auto&& type_val) {
                    // Check if this is a table and represents an individual satellite
                    if constexpr(toml::is_table<decltype(type_val)>) {
                        Dictionary dict_name;
                        LOG(config_parser_logger_, DEBUG) << "Found satellite name sub-node " << type_key;

                        type_val.as_table()->for_each([&](const toml::key& name_key, auto&& name_val) {
                            LOG(config_parser_logger_, DEBUG) << "Reading name key " << name_key;

                            const auto value = parse_value(name_key, name_val);
                            if(value.has_value()) {
                                // Insert or assign - these keys always take priority
                                dict_name.emplace(std::string(name_key.str()), value.value());
                            }
                        });

                        // Add dictionary for satellite
                        satellite_configs_.emplace(type_key, dict_name);

                    } else {
                        LOG(config_parser_logger_, DEBUG) << "Reading type key " << type_key;

                        const auto value = parse_value(type_key, type_val);
                        if(value.has_value()) {
                            dict_type.emplace(std::string(type_key.str()), value.value());
                        }
                    }
                });

                // Add type dictionary:
                type_configs_.emplace(global_key, dict_type);
            } else {
                LOG(config_parser_logger_, DEBUG) << "Reading satellites key " << global_key;
                const auto value = parse_value(global_key, global_val);
                if(value.has_value()) {
                    global_config_.emplace(std::string(global_key.str()), value.value());
                }
            }
        });

    } else {
        LOG(config_parser_logger_, WARNING) << "Could not find base node for satellites";
    }
}

std::map<std::string, Dictionary>
ControllerConfiguration::getSatelliteConfigurations(std::set<std::string> canonical_names) const {
    std::map<std::string, Dictionary> configs;

    for(const auto& name : canonical_names) {
        const auto dictionary = getSatelliteConfiguration(name);
        if(dictionary.has_value()) {
            configs.insert({name, dictionary.value()});
        }
    }

    return configs;
}

std::optional<Dictionary> ControllerConfiguration::getSatelliteConfiguration(std::string_view canonical_name) const {

    const auto separator = canonical_name.find_first_of('.');
    const auto type = canonical_name.substr(0, separator);
    const auto name = canonical_name.substr(separator + 1);

    // We need to look for canonical name - and case-insensitive
    const auto cfg_it = std::ranges::find_if(satellite_configs_, [canonical_name](const auto& r) {
        return transform(r.first, ::tolower) == transform(canonical_name, ::tolower);
    });
    if(cfg_it != satellite_configs_.end()) {
        auto config = cfg_it->second;

        // Add type keys
        const auto type_it = type_configs_.find(type);
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

    return std::nullopt;
}
