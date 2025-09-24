/**
 * @file
 * @brief Implementation of configuration parser
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ControllerConfiguration.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <version> // IWYU pragma: keep

#if __cpp_lib_chrono < 201907L
#include <ctime>
#endif

#include <toml++/toml.hpp>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::controller;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::protocol;
using namespace constellation::utils;

ControllerConfiguration::ControllerConfiguration(const std::filesystem::path& path) {
    // Check if file exists
    if(!std::filesystem::is_regular_file(path)) {
        throw ConfigFileNotFoundError(path);
    }

    // Convert to absolute path
    const auto file_path_abs = std::filesystem::canonical(path);
    LOG(config_parser_logger_, DEBUG) << "Parsing configuration file " << quote(file_path_abs.string());

    const std::ifstream file {file_path_abs};
    std::ostringstream buffer {};
    buffer << file.rdbuf();

    parse_toml(buffer.view());

    // Build the dependency graph from all satellite configurations
    for(const auto& [name, cfg] : satellite_configs_) {
        fill_dependency_graph(name);
    }

    // Validate the configuration
    validate();
}

ControllerConfiguration::ControllerConfiguration(std::string_view toml) {
    parse_toml(toml);

    // Build the dependency graph from all satellite configurations
    for(const auto& [name, cfg] : satellite_configs_) {
        fill_dependency_graph(name);
    }

    // Validate the configuration
    validate();
}

std::string ControllerConfiguration::getAsTOML() const {

    // Validate the configuration
    try {
        validate();
    } catch(const ConfigFileValidationError& error) {
        LOG(config_parser_logger_, WARNING) << error.what();
    }

    auto get_toml_array = [&](auto&& val) -> toml::array {
        toml::array arr;

        using T = std::decay_t<decltype(val)>::value_type;
        for(const auto& v : val) {
            arr.push_back(static_cast<T>(v));
        }
        return arr;
    };

    auto get_toml_table = [&](const config::Dictionary& dict) -> toml::table {
        toml::table tbl;
        for(const auto& [key, value] : dict) {

            LOG(config_parser_logger_, TRACE) << "Parsing key " << key;

            if(std::holds_alternative<bool>(value)) {
                tbl.emplace(key, toml::value<bool>(value.get<bool>()));
            } else if(std::holds_alternative<std::int64_t>(value)) {
                tbl.emplace(key, toml::value<std::int64_t>(value.get<std::int64_t>()));
            } else if(std::holds_alternative<double>(value)) {
                tbl.emplace(key, toml::value<double>(value.get<double>()));
            } else if(std::holds_alternative<std::string>(value)) {
                tbl.emplace(key, toml::value<std::string>(value.get<std::string>()));
            } else if(std::holds_alternative<std::vector<bool>>(value)) {
                tbl.emplace(key, get_toml_array(value.get<std::vector<bool>>()));
            } else if(std::holds_alternative<std::vector<std::string>>(value)) {
                tbl.emplace(key, get_toml_array(value.get<std::vector<std::string>>()));
            } else if(std::holds_alternative<std::vector<double>>(value)) {
                tbl.emplace(key, get_toml_array(value.get<std::vector<double>>()));
            } else if(std::holds_alternative<std::vector<std::int64_t>>(value)) {
                tbl.emplace(key, get_toml_array(value.get<std::vector<std::int64_t>>()));
            }

            // FIXME timestamp? char vector?
        }
        return tbl;
    };

    // The global TOML table
    toml::table tbl;

    // Add global config:
    tbl.emplace("satellites", get_toml_table(global_config_));

    // Add type config:
    for(const auto& [type, config] : type_configs_) {
        tbl["satellites"].as_table()->emplace(type, get_toml_table(config));
    }

    // Add individual satellites sections:
    for(const auto& [canonical_name, config] : satellite_configs_) {
        const auto pos = canonical_name.find_first_of('.', 0);
        const auto type = canonical_name.substr(0, pos);
        const auto name = canonical_name.substr(pos + 1);
        tbl["satellites"].as_table()->emplace(type, toml::table {});
        tbl["satellites"][type].as_table()->emplace(name, get_toml_table(config));
    }

    // Clean up table a bit:
    tbl.prune();

    std::stringstream oss;
    oss << tbl << "\n";

    return oss.str();
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
        if constexpr(toml::is_time<decltype(val)>) {
            // TOML defines this as local time
            const auto toml_time = val.as_time()->get();

            const auto time_now = std::chrono::system_clock::now();

#if __cpp_lib_chrono >= 201907L
            const auto local_time_now = std::chrono::current_zone()->to_local(time_now);
            // Use midnight today plus local time from TOML
            const auto local_time = std::chrono::floor<std::chrono::days>(local_time_now) +
                                    std::chrono::hours {toml_time.hour} + std::chrono::minutes {toml_time.minute} +
                                    std::chrono::seconds {toml_time.second};
            const auto system_time = std::chrono::current_zone()->to_sys(local_time);
#else
            const auto time_t = std::chrono::system_clock::to_time_t(time_now);
            std::tm tm {};
            localtime_r(&time_t, &tm); // there is no thread-safe std::localtime
            // Use mightnight today plus local time from TOML
            tm.tm_hour = toml_time.hour;
            tm.tm_min = toml_time.minute;
            tm.tm_sec = toml_time.second;
            const auto local_time_t = std::mktime(&tm);
            const auto system_time = std::chrono::system_clock::from_time_t(local_time_t);
#endif

            // Return as system time
            return system_time;
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

void ControllerConfiguration::fill_dependency_graph(std::string_view canonical_name) {
    // Parse all transition condition parameters
    for(const auto& state : {CSCP::State::initializing,
                             CSCP::State::launching,
                             CSCP::State::landing,
                             CSCP::State::starting,
                             CSCP::State::stopping}) {
        const auto key = transform("_require_" + to_string(state) + "_after", ::tolower);

        // Get final assembled config and look for the key:
        const auto config = getSatelliteConfiguration(canonical_name);
        if(config.contains(key)) {
            auto register_dependency = [this, canonical_name, state](auto& dependent) {
                // Register dependency in graph, current satellite depends on config value satellite
                transition_graph_[state][transform(dependent, ::tolower)].emplace(transform(canonical_name, ::tolower));
            };

            const auto dependents = Configuration(config).getArray<std::string>(key);
            LOG(config_parser_logger_, DEBUG)
                << "Registering dependency for transitional state " << quote(to_string(state)) << " of " << canonical_name
                << " with dependents " << range_to_string(dependents);
            std::ranges::for_each(dependents, register_dependency);
        }
    }
}

void ControllerConfiguration::validate() const {

    // Check each transition for possible cycles
    for(const auto& transition_pair : transition_graph_) {
        const auto transition = transition_pair.first;
        LOG(config_parser_logger_, DEBUG) << "Checking for deadlock in transition: " << transition;

        if(check_transition_deadlock(transition)) {
            LOG(config_parser_logger_, DEBUG) << "Deadlock detected in transition: " << transition;
            throw ConfigFileValidationError("Cyclic dependency for transition " + quote(to_string(transition)));
        }
    }
    // No deadlock in any transition
}

bool ControllerConfiguration::check_transition_deadlock(CSCP::State transition) const {
    // If no dependencies for this transition doesn't exist, there is no cycle:
    if(!transition_graph_.contains(transition)) {
        return false;
    }

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursion_stack;

    // Recursive depth first search:
    auto dfs = [&](const auto& self, const std::string& satellite) { // NOLINT(misc-no-recursion)
        // Cycle detected (deadlock)
        if(recursion_stack.contains(satellite)) {
            return true;
        }

        // Satellite already processed
        if(visited.contains(satellite)) {
            return false;
        }

        // No transition registered for this satellite:
        if(!transition_graph_.at(transition).contains(satellite)) {
            return false;
        }

        visited.insert(satellite);
        recursion_stack.insert(satellite);

        // Visit all dependent satellites
        for(const auto& dependent : transition_graph_.at(transition).at(satellite)) {
            if(self(self, dependent)) {
                return true;
            }
        }

        // Remove satellite from recursion stack
        recursion_stack.erase(satellite);
        return false;
    };

    // Traverse each satellite for the given transition
    const auto deadlock = std::ranges::any_of(transition_graph_.at(transition), [&](const auto& p) {
        const std::string& satellite = p.first;
        if(!visited.contains(satellite)) {
            if(dfs(dfs, satellite)) {
                // Deadlock detected in this transition
                return true;
            }
        }
        return false;
    });

    return deadlock;
}

bool ControllerConfiguration::hasSatelliteConfiguration(std::string_view canonical_name) const {
    return satellite_configs_.contains(transform(canonical_name, ::tolower));
}

void ControllerConfiguration::addSatelliteConfiguration(std::string_view canonical_name, config::Dictionary config) {

    // Check if already there
    const auto canonical_name_lc = transform(canonical_name, ::tolower);
    auto config_it = satellite_configs_.find(canonical_name_lc);
    if(config_it != satellite_configs_.end()) {
        LOG(config_parser_logger_, WARNING) << "Overwriting existing satellite configuration for " << canonical_name;
        config_it->second = std::move(config);
    } else {
        satellite_configs_.emplace(canonical_name_lc, std::move(config));
    }

    // Add satellite to dependency graph:
    fill_dependency_graph(canonical_name);
}

Dictionary ControllerConfiguration::getSatelliteConfiguration(std::string_view canonical_name) const {
    LOG(config_parser_logger_, TRACE) << "Fetching configuration for " << canonical_name;

    // Find type from canonical name
    const auto separator_pos = canonical_name.find_first_of('.');
    const auto type = canonical_name.substr(0, separator_pos);

    // Copy global section
    auto config = global_config_;

    // Add parameters from type section (dict always stores types in lower case)
    const auto type_it = type_configs_.find(transform(type, ::tolower));
    if(type_it != type_configs_.end()) {
        LOG(config_parser_logger_, TRACE) << "Found type section for " << type;
        for(const auto& [key, value] : type_it->second) {
            const auto param_it = config.find(key);
            if(param_it != config.end()) {
                // Overwrite existing parameter
                param_it->second = value;
                LOG(config_parser_logger_, DEBUG) << "Overwritten " << key << " from type section for " << type;
            } else {
                config.emplace(key, value);
            }
        }
    }

    // Add parameters from satellite section (dict always stores names in lower case)
    const auto satellite_it = satellite_configs_.find(transform(canonical_name, ::tolower));
    if(satellite_it != satellite_configs_.end()) {
        LOG(config_parser_logger_, TRACE) << "Found named section for " << type;
        for(const auto& [key, value] : satellite_it->second) {
            const auto param_it = config.find(key);
            if(param_it != config.end()) {
                // Overwrite existing parameter
                param_it->second = value;
                LOG(config_parser_logger_, DEBUG) << "Overwritten " << key << " from named section for " << canonical_name;
            } else {
                config.emplace(key, value);
            }
        }
    }

    return config;
}
