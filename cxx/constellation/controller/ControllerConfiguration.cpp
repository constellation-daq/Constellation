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
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>

#include <toml++/toml.hpp>
#include <yaml-cpp/yaml.h>

#include "constellation/controller/exceptions.hpp"
#include "constellation/controller/toml_helpers.hpp"
#include "constellation/controller/yaml_helpers.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/value_types.hpp"
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

    const auto type = detect_config_type(path);
    switch(type) {
    case FileType::YAML: {
        parse_yaml(buffer.view());
        break;
    }
    case FileType::UNKNOWN:
    case FileType::TOML: {
        parse_toml(buffer.view());
        break;
    }
    default: std::unreachable();
    }

    // Validate the configuration
    validate();
}

ControllerConfiguration::ControllerConfiguration(std::string_view config, ControllerConfiguration::FileType type) {

    switch(type) {
    case FileType::YAML: {
        parse_yaml(config);
        break;
    }
    case FileType::UNKNOWN:
    case FileType::TOML: {
        parse_toml(config);
        break;
    }
    default: std::unreachable();
    }

    // Validate the configuration
    validate();
}

ControllerConfiguration::FileType ControllerConfiguration::detect_config_type(const std::filesystem::path& file) {
    const auto ext = transform(file.extension().string(), ::tolower);

    if(ext == ".yaml" || ext == ".yml") {
        return FileType::YAML;
    }
    if(ext == ".toml") {
        return FileType::TOML;
    }

    return FileType::UNKNOWN;
}

std::string ControllerConfiguration::getAsYAML() const {
    YAML::Emitter out;

    // Use flow-style for arrays
    out.SetSeqFormat(YAML::Flow);

    // Global dictionary
    out << YAML::BeginMap;

    // Add the global configuration keys
    if(!global_config_.empty()) {
        YAML::Node global_node {};
        for(const auto& [key, value] : global_config_) {
            global_node[key] = get_as_yaml_node(value);
        }
        out << YAML::Key << "_default";
        out << YAML::Value << global_node;
    }

    // Cache type nodes for later modification
    std::map<std::string, YAML::Node> type_nodes {};

    // Add type config
    for(const auto& [type, config] : type_configs_) {
        auto [type_node_it, inserted] = type_nodes.emplace(type, YAML::Node());
        auto& type_node = type_node_it->second;

        // Add type config keys
        YAML::Node node {};
        for(const auto& [key, value] : config) {
            node[key] = get_as_yaml_node(value);
        }
        type_node["_default"] = node;
    }

    // Append satellite configs to the type nodes:
    for(const auto& [canonical_name, config] : satellite_configs_) {
        const auto pos = canonical_name.find_first_of('.', 0);
        const auto type = canonical_name.substr(0, pos);
        const auto name = canonical_name.substr(pos + 1);

        auto [type_node_it, inserted] = type_nodes.emplace(type, YAML::Node());
        auto& type_node = type_node_it->second;

        // Add satellite config keys
        YAML::Node node {};
        for(const auto& [key, value] : config) {
            node[key] = get_as_yaml_node(value);
        }
        type_node[name] = node;
    }

    // Write final type nodes
    for(const auto& [key, node] : type_nodes) {
        out << YAML::Key << key;
        out << YAML::Value << node;
    }

    LOG_IF(config_parser_logger_, WARNING, !out.good()) << "Emitter error: " << out.GetLastError();

    // End global dictionary
    out << YAML::EndMap;
    return out.c_str();
}

std::string ControllerConfiguration::getAsTOML() const {
    // The global TOML table
    toml::table tbl {};
    tbl.emplace("_default", get_as_toml_table(global_config_));

    // Add type config
    for(const auto& [type, config] : type_configs_) {
        const auto [type_table_it, inserted] = tbl.emplace(type, toml::table());
        type_table_it->second.as_table()->emplace("_default", get_as_toml_table(config));
    }

    // Add config from individual satellites
    for(const auto& [canonical_name, config] : satellite_configs_) {
        const auto pos = canonical_name.find_first_of('.', 0);
        const auto type = canonical_name.substr(0, pos);
        const auto name = canonical_name.substr(pos + 1);
        const auto [type_table_it, inserted] = tbl.emplace(type, toml::table());
        type_table_it->second.as_table()->emplace(name, get_as_toml_table(config));
    }

    std::stringstream oss;
    oss << tbl << '\n';

    return oss.str();
}

void ControllerConfiguration::parse_yaml(std::string_view yaml) {
    YAML::Node root_node {};
    try {
        root_node = YAML::Load(std::string(yaml));
    } catch(const YAML::ParserException& e) {
        throw ConfigParseError(e.what());
    }

    // Root node needs to be a map or empty:
    if(!root_node.IsMap() && !root_node.IsNull()) {
        throw ConfigParseError("expected map as root node");
    }

    // Bool to check if global default config is defined multiple times
    bool has_global_default_config = false;

    // Loop over all nodes
    for(const auto& type_node_it : root_node) {
        const auto type_key_lc = parse_yaml_key(type_node_it);
        const auto& type_node = type_node_it.second;
        if(type_node.IsNull()) {
            // Skip if empty
            continue;
        }
        if(type_node.IsMap()) {
            if(type_key_lc == "_default") {
                // Global default config
                LOG(config_parser_logger_, DEBUG) << "Found default config at global level";
                if(has_global_default_config) {
                    throw ConfigKeyError(type_key_lc, "key defined twice");
                }
                global_config_ = parse_yaml_map(type_key_lc, type_node);
                has_global_default_config = true;
            } else {
                // Type level
                if(!CSCP::is_valid_satellite_type(type_key_lc)) {
                    throw ConfigKeyError(type_key_lc, "not a valid satellite type");
                }
                LOG(config_parser_logger_, DEBUG) << "Found type level for " << quote(type_key_lc);
                for(const auto& name_node_it : type_node) {
                    const auto name_key_lc = parse_yaml_key(name_node_it);
                    const auto canonical_name_key_lc = (type_key_lc + '.').append(name_key_lc);
                    const auto& name_node = name_node_it.second;
                    if(name_node.IsNull() && name_key_lc != "_default") {
                        // If node is empty, emplace empty satellite config and continue to next node
                        const auto [it, inserted] = satellite_configs_.try_emplace(canonical_name_key_lc);
                        if(!inserted) {
                            throw ConfigKeyError(canonical_name_key_lc, "key defined twice");
                        }
                        continue;
                    }
                    if(name_node.IsMap()) {
                        if(name_key_lc == "_default") {
                            // Type default config
                            LOG(config_parser_logger_, DEBUG)
                                << "Found default config at type level for " << quote(type_key_lc);
                            const auto [it, inserted] =
                                type_configs_.try_emplace(type_key_lc, parse_yaml_map(canonical_name_key_lc, name_node));
                            if(!inserted) {
                                throw ConfigKeyError(canonical_name_key_lc, "key defined twice");
                            }
                        } else {
                            // Satellite level
                            if(!CSCP::is_valid_satellite_name(name_key_lc)) {
                                throw ConfigKeyError(canonical_name_key_lc, "not a valid satellite name");
                            }
                            LOG(config_parser_logger_, DEBUG)
                                << "Found config at satellite level for " << quote(canonical_name_key_lc);
                            const auto [it, inserted] = satellite_configs_.try_emplace(
                                canonical_name_key_lc, parse_yaml_map(canonical_name_key_lc, name_node));
                            if(!inserted) {
                                throw ConfigKeyError(canonical_name_key_lc, "key defined twice");
                            }
                        }
                    } else {
                        throw ConfigValueError(canonical_name_key_lc, "expected a dictionary at satellite level");
                    }
                }
            }
        } else {
            throw ConfigValueError(type_key_lc, "expected a dictionary at type level");
        }
    }
}

void ControllerConfiguration::parse_toml(std::string_view toml) {
    toml::table tbl {};
    try {
        tbl = toml::parse(toml);
    } catch(const toml::parse_error& err) {
        std::ostringstream oss {};
        oss << err;
        throw ConfigParseError(oss.view());
    }

    // Bool to check if global default config is defined multiple times
    bool has_global_default_config = false;

    // Loop over all nodes
    tbl.for_each([this, &has_global_default_config](const toml::key& type_key, auto&& type_value) {
        const auto type_key_lc = transform(type_key.str(), ::tolower);
        if constexpr(toml::is_table<decltype(type_value)>) {
            if(type_key_lc == "_default") {
                // Global default config
                LOG(config_parser_logger_, DEBUG) << "Found default config at global level";
                if(has_global_default_config) {
                    throw ConfigKeyError(type_key_lc, "key defined twice");
                }
                global_config_ = parse_toml_table(type_key_lc, std::forward<decltype(type_value)>(type_value));
                has_global_default_config = true;
            } else {
                // Type level
                if(!CSCP::is_valid_satellite_type(type_key_lc)) {
                    throw ConfigKeyError(type_key_lc, "not a valid satellite type");
                }
                LOG(config_parser_logger_, DEBUG) << "Found type level for " << quote(type_key_lc);
                type_value.for_each([this, &type_key_lc](const toml::key& name_key, auto&& name_value) {
                    const auto name_key_lc = transform(name_key.str(), ::tolower);
                    const auto canonical_name_key_lc = (type_key_lc + '.').append(name_key_lc);
                    if constexpr(toml::is_table<decltype(name_value)>) {
                        if(name_key_lc == "_default") {
                            // Type default config
                            LOG(config_parser_logger_, DEBUG)
                                << "Found default config at type level for " << quote(type_key_lc);
                            const auto [it, inserted] = type_configs_.try_emplace(
                                type_key_lc,
                                parse_toml_table(canonical_name_key_lc, std::forward<decltype(name_value)>(name_value)));
                            if(!inserted) {
                                throw ConfigKeyError(canonical_name_key_lc, "key defined twice");
                            }
                        } else {
                            // Satellite level
                            if(!CSCP::is_valid_satellite_name(name_key_lc)) {
                                throw ConfigKeyError(canonical_name_key_lc, "not a valid satellite name");
                            }
                            LOG(config_parser_logger_, DEBUG)
                                << "Found config at satellite level for " << quote(canonical_name_key_lc);
                            const auto [it, inserted] = satellite_configs_.try_emplace(
                                canonical_name_key_lc,
                                parse_toml_table(canonical_name_key_lc, std::forward<decltype(name_value)>(name_value)));
                            if(!inserted) {
                                throw ConfigKeyError(canonical_name_key_lc, "key defined twice");
                            }
                        }
                    } else {
                        throw ConfigValueError(canonical_name_key_lc, "expected a dictionary at satellite level");
                    }
                });
            }
        } else {
            throw ConfigValueError(type_key_lc, "expected a dictionary at type level");
        }
    });
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

void ControllerConfiguration::validate() {
    // Build the dependency graph from all satellite configurations
    for(const auto& [name, cfg] : satellite_configs_) {
        fill_dependency_graph(name);
    }

    // Check each transition for possible cycles
    for(const auto& transition_pair : transition_graph_) {
        const auto transition = transition_pair.first;
        LOG(config_parser_logger_, DEBUG) << "Checking for deadlock in transition: " << transition;

        if(check_transition_deadlock(transition)) {
            LOG(config_parser_logger_, DEBUG) << "Deadlock detected in transition: " << transition;
            throw ConfigValidationError("Cyclic dependency for transition " + quote(to_string(transition)));
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

// NOLINTNEXTLINE(misc-no-recursion)
void ControllerConfiguration::overwrite_config(const std::string& key_prefix,
                                               Dictionary& base_config,
                                               const Dictionary& config) const {
    for(const auto& [key, value] : config) {
        auto base_kv_it = base_config.find(key);
        if(base_kv_it != base_config.cend()) {
            // Overwrite existing parameter
            const auto prefixed_key = key_prefix + key;
            auto& base_value = base_kv_it->second;
            // Ensure that dictionary types match
            if((std::holds_alternative<Dictionary>(value) || std::holds_alternative<Dictionary>(base_value)) &&
               (value.index() != base_value.index())) {
                throw ConfigValidationError("value of key " + quote(prefixed_key) +
                                            " has mismatched types when merging defaults");
            }
            if(std::holds_alternative<Dictionary>(base_value)) {
                // If dictionary, overwrite recursively
                overwrite_config(prefixed_key + '.', base_value.get<Dictionary>(), value.get<Dictionary>());
            } else {
                // Otherwise overwrite directly (ignore type mismatch)
                base_value = value;
                LOG(config_parser_logger_, TRACE) << "Overwritten value for key " << quote(prefixed_key);
            }
        } else {
            // Add new parameter
            base_config.emplace(key, value);
        }
    }
}

bool ControllerConfiguration::hasTypeConfiguration(std::string_view type) const {
    return type_configs_.contains(transform(type, ::tolower));
}

void ControllerConfiguration::addTypeConfiguration(std::string_view type, Dictionary config) {
    // Check if already there
    const auto type_lc = transform(type, ::tolower);
    auto config_it = type_configs_.find(type_lc);
    if(config_it != type_configs_.cend()) {
        LOG(config_parser_logger_, WARNING) << "Overwriting existing satellite type configuration for " << quote(type);
        config_it->second = std::move(config);
    } else {
        type_configs_.emplace(type_lc, std::move(config));
    }
}

Dictionary ControllerConfiguration::getTypeConfiguration(std::string_view type) const {
    LOG(config_parser_logger_, TRACE) << "Fetching configuration for type " << quote(type);

    // Copy global config
    auto config = getGlobalConfiguration();

    // Add parameters from type level
    const auto type_lc = transform(type, ::tolower);
    const auto config_it = type_configs_.find(transform(type_lc, ::tolower));
    if(config_it != type_configs_.end()) {
        LOG(config_parser_logger_, TRACE) << "Found config at type level for " << quote(type);
        overwrite_config("", config, config_it->second);
    }

    return config;
}

bool ControllerConfiguration::hasSatelliteConfiguration(std::string_view canonical_name) const {
    return satellite_configs_.contains(transform(canonical_name, ::tolower));
}

void ControllerConfiguration::addSatelliteConfiguration(std::string_view canonical_name, Dictionary config) {
    // Check if already there
    const auto canonical_name_lc = transform(canonical_name, ::tolower);
    auto config_it = satellite_configs_.find(canonical_name_lc);
    if(config_it != satellite_configs_.end()) {
        LOG(config_parser_logger_, WARNING) << "Overwriting existing satellite configuration for " << quote(canonical_name);
        config_it->second = std::move(config);
    } else {
        satellite_configs_.emplace(canonical_name_lc, std::move(config));
    }
}

Dictionary ControllerConfiguration::getSatelliteConfiguration(std::string_view canonical_name) const {
    LOG(config_parser_logger_, TRACE) << "Fetching configuration for " << quote(canonical_name);

    // Find type from canonical name
    const auto canonical_name_lc = transform(canonical_name, ::tolower);
    const auto separator_pos = canonical_name_lc.find_first_of('.');
    const auto type_lc = canonical_name_lc.substr(0, separator_pos);

    // Copy from global global + type level
    auto config = getTypeConfiguration(type_lc);

    // Add parameters from satellite level
    const auto config_it = satellite_configs_.find(canonical_name_lc);
    if(config_it != satellite_configs_.end()) {
        LOG(config_parser_logger_, TRACE) << "Found config at satellite level for " << quote(canonical_name);
        overwrite_config("", config, config_it->second);
    }

    return config;
}
