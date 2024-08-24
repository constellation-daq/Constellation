/**
 * @file
 * @brief Implementation of configuration parser
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ConfigParser.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include <toml++/toml.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::controller;
using namespace constellation::config;
using namespace constellation::log;

Logger ConfigParser::config_parser_logger_("CFGPARSER");

std::string ConfigParser::read_file(std::ifstream file) {
    LOG(config_parser_logger_, DEBUG) << "Parsing configuration file";
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::map<std::string, Dictionary> ConfigParser::getDictionaries(std::set<std::string> satellites, std::string_view toml) {
    return parse_config(std::move(satellites), toml);
}

std::optional<Dictionary> ConfigParser::getDictionary(const std::string& satellite, std::string_view toml) {
    const auto configs = parse_config({satellite}, toml);
    if(configs.contains(satellite)) {
        return configs.at(utils::transform(satellite, ::tolower));
    }
    return std::nullopt;
}

std::map<std::string, Dictionary> ConfigParser::getDictionaries(std::set<std::string> satellites, std::ifstream file) {
    const auto buffer = read_file(std::move(file));
    return parse_config(std::move(satellites), buffer);
}

std::optional<Dictionary> ConfigParser::getDictionary(const std::string& satellite, std::ifstream file) {
    const auto buffer = read_file(std::move(file));
    const auto configs = parse_config({satellite}, buffer);
    if(configs.contains(satellite)) {
        return configs.at(utils::transform(satellite, ::tolower));
    }
    return std::nullopt;
}

std::map<std::string, Dictionary> ConfigParser::parse_config(std::set<std::string> satellites, std::string_view toml) {

    toml::table tbl {};
    try {
        tbl = toml::parse(toml);
    } catch(const toml::parse_error& err) {
        std::stringstream s;
        s << err;
        throw std::invalid_argument(s.str());
    }

    auto parse_value = [&](const toml::key& key, auto&& val) -> std::optional<Value> {
        if constexpr(toml::is_table<decltype(val)>) {
            LOG(config_parser_logger_, DEBUG) << "Skipping table for key " << key;
            return {};
        } else if constexpr(toml::is_array<decltype(val)>) {
            if(val.is_homogeneous()) {
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
                    LOG(config_parser_logger_, WARNING) << "Unknown type of array for key " << key;
                    // throw
                    return {};
                }
            } else {
                LOG(config_parser_logger_, WARNING) << "Array with key " << key << " is not homogeneous";
                // throw
                return {};
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
                LOG(config_parser_logger_, WARNING) << "Unknown value type for key " << key;
                // throw
                return {};
            }
        }
    };

    std::map<std::string, config::Dictionary> configs {};
    for(const auto& sat : satellites) {
        // Start with empty dictionary:
        configs.emplace(sat, Dictionary {});

        const auto separator = sat.find_first_of('.');
        const auto type = sat.substr(0, separator);
        const auto name = sat.substr(separator + 1);

        // Find satellites base node and add keys
        if(const auto& node = tbl.at_path("satellites")) {
            Dictionary dict_all;
            Dictionary dict_type;

            // Write individual keys if not present yet:
            node.as_table()->for_each([&](const toml::key& key, auto&& val) {
                // Check if this is a table for this satellite type
                if constexpr(toml::is_table<decltype(val)>) {
                    if(utils::transform(key, ::tolower) == utils::transform(type, ::tolower)) {

                        LOG(config_parser_logger_, DEBUG) << "Found satellite type sub-node " << key;
                        val.as_table()->for_each([&](const toml::key& key, auto&& val) {
                            // Check if this is a table for this satellite name
                            if constexpr(toml::is_table<decltype(val)>) {
                                if(utils::transform(key, ::tolower) == utils::transform(name, ::tolower)) {
                                    LOG(config_parser_logger_, DEBUG) << "Found satellite name sub-node " << key;
                                    val.as_table()->for_each([&](const toml::key& key, auto&& val) {
                                        LOG(config_parser_logger_, DEBUG) << "Reading name key " << key;

                                        auto value = parse_value(key, val);
                                        if(value.has_value()) {
                                            // Insert or assign - these keys always take priority
                                            configs[sat].emplace(std::string(key.str()), value.value());
                                        }
                                    });
                                }
                            } else {
                                LOG(config_parser_logger_, DEBUG) << "Reading type key " << key;

                                auto value = parse_value(key, val);
                                if(value.has_value()) {
                                    dict_type.emplace(std::string(key.str()), value.value());
                                }
                            }
                        });
                    }
                } else {
                    LOG(config_parser_logger_, DEBUG) << "Reading satellites key " << key;
                    auto value = parse_value(key, val);
                    if(value.has_value()) {
                        dict_all.emplace(std::string(key.str()), value.value());
                    }
                }
            });

            // Combine dictionaries, do not overwrite existing keys:
            for(const auto& [key, value] : dict_type) {
                const auto& [it, inserted] = configs[sat].insert({key, value});
                LOG_IF(config_parser_logger_, DEBUG, inserted) << "Added key " << key << " from type section";
            }

            for(const auto& [key, value] : dict_all) {
                const auto& [it, inserted] = configs[sat].insert({key, value});
                LOG_IF(config_parser_logger_, DEBUG, inserted) << "Added key " << key << " from global satellites section";
            }

        } else {
            LOG(config_parser_logger_, WARNING) << "Could not find base node for satellites";
        }
    }

    return configs;
}
