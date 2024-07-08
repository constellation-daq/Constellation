/**
 * @file
 * @brief Configuration parser implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Parser.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::config;

ConfigParser::ConfigParser() = default;
ConfigParser::ConfigParser(std::istream& stream, std::filesystem::path file_name) : ConfigParser() {
    add(stream, std::move(file_name));
}

/**
 * @throws KeyValueParseError If the key / value pair could not be parsed
 *
 * The key / value pair is split according to the format specifications
 */
std::pair<std::string, Value> ConfigParser::parseKeyValue(std::string line) {
    line = utils::trim(line);
    size_t equals_pos = line.find('=');
    if(equals_pos != std::string::npos) {
        std::string key = utils::trim(std::string(line, 0, equals_pos));
        std::string value = utils::trim(std::string(line, equals_pos + 1));
        char last_quote = 0;
        for(size_t i = 0; i < value.size(); ++i) {
            if(value[i] == '\'' || value[i] == '\"') {
                if(last_quote == 0) {
                    last_quote = value[i];
                } else if(last_quote == value[i]) {
                    last_quote = 0;
                }
            }
            if(last_quote == 0 && value[i] == '#') {
                value = std::string(value, 0, i);
                break;
            }
        }

        // Check if key contains only alphanumeric or underscores
        bool valid_key = true;
        for(auto& ch : key) {
            if(isalnum(ch) == 0 && ch != '_' && ch != '.' && ch != ':') {
                valid_key = false;
                break;
            }
        }

        // Check if value is not empty and key is valid
        if(!valid_key) {
            throw KeyValueParseError(line, "key is not valid");
        }

        if(value.empty()) {
            return std::make_pair(key, std::monostate {});
        }

        // Try to convert value into target type. Order matters since an integer will also correctly evaluate to a double
        auto parsed_value = parse_value(value);

        // Attempt conversion to integer:
        std::int64_t int_value {};
        if(std::from_chars(parsed_value->value.data(), parsed_value->value.data() + parsed_value->value.size(), int_value)
               .ec == std::errc {}) {
            return std::make_pair(key, int_value);
        }

        // Attempt conversion to float:
        double float_value {};
        if(std::from_chars(parsed_value->value.data(), parsed_value->value.data() + parsed_value->value.size(), float_value)
               .ec == std::errc {}) {
            return std::make_pair(key, float_value);
        }

        // Attempt conversion to boolean:
        bool bool_value {};
        std::istringstream is(utils::transform(parsed_value->value, ::tolower));
        is >> std::boolalpha >> bool_value;
        if(!is.fail()) {
            return std::make_pair(key, bool_value);
        }

        // Otherwise return as string:
        return std::make_pair(key, parsed_value->value);
    }

    // Key / value pair does not contain equal sign
    throw KeyValueParseError(line, "missing equality sign to split key and value");
}

/**
 * String is recursively parsed for all pair of [ and ] brackets. All parts between single or double quotation marks are
 * skipped.
 */
std::unique_ptr<ConfigParser::parse_node> ConfigParser::parse_value(std::string str, int depth) { // NOLINT

    auto node = std::make_unique<parse_node>();
    str = utils::trim(str);
    if(str.empty()) {
        throw std::invalid_argument("element is empty");
    }

    // Initialize variables for non-zero levels
    size_t beg = 1, lst = 1, in_dpt = 0;
    bool in_dpt_chg = false;

    // Implicitly add pair of brackets on zero level
    if(depth == 0) {
        beg = lst = 0;
        in_dpt = 1;
    }

    for(size_t i = 0; i < str.size(); ++i) {
        // Skip over quotation marks
        if(str[i] == '\'' || str[i] == '\"') {
            i = str.find(str[i], i + 1);
            if(i == std::string::npos) {
                throw std::invalid_argument("quotes are not balanced");
            }
            continue;
        }

        // Handle brackets
        if(str[i] == '[') {
            ++in_dpt;
            if(!in_dpt_chg && i != 0) {
                throw std::invalid_argument("invalid start bracket");
            }
            in_dpt_chg = true;
        } else if(str[i] == ']') {
            if(in_dpt == 0) {
                throw std::invalid_argument("brackets are not matched");
            }
            --in_dpt;
            in_dpt_chg = true;
        }

        // Make subitems at the zero level
        if(in_dpt == 1 && (str[i] == ',' || (isspace(str[i]) != 0 && (isspace(str[i - 1]) == 0 && str[i - 1] != ',')))) {
            node->children.push_back(parse_value(str.substr(lst, i - lst), depth + 1));
            lst = i + 1;
        }
    }

    if((depth > 0 && in_dpt != 0) || (depth == 0 && in_dpt != 1)) {
        throw std::invalid_argument("brackets are not balanced");
    }

    // Determine if array or value
    if(in_dpt_chg || depth == 0) {
        // Handle last array item
        size_t end = str.size();
        if(depth != 0) {
            if(str.back() != ']') {
                throw std::invalid_argument("invalid end bracket");
            }
            end = str.size() - 1;
        }
        node->children.push_back(parse_value(str.substr(lst, end - lst), depth + 1));
        node->value = str.substr(beg, end - beg);
    } else {
        // Not an array, handle as value instead
        node->value = std::move(str);
    }

    // Handle zero level where brackets where explicitly added
    if(depth == 0 && node->children.size() == 1 && !node->children.front()->children.empty()) {
        node = std::move(node->children.front());
    }

    return node;
}

/**
 * @throws ConfigParseError If an error occurred during the parsing of the stream
 *
 * The configuration is immediately parsed and all of its configurations are available after the functions returns.
 */
void ConfigParser::add(std::istream& stream, std::filesystem::path file_name) {

    // Convert file name to absolute path (if given)
    if(!file_name.empty()) {
        file_name = std::filesystem::canonical(file_name);
    }

    // Build first empty configuration
    std::string section_name;
    Configuration conf;

    int line_num = 0;
    while(true) {
        // Process config line by line
        std::string line;
        if(stream.eof()) {
            break;
        }
        std::getline(stream, line);
        ++line_num;

        // Trim whitespaces at beginning and end of line:
        line = utils::trim(line);

        // Ignore empty lines or comments
        if(line.empty() || line.front() == '#') {
            continue;
        }

        // Check if section header or key-value pair
        if(line.front() == '[') {
            // Line should be a section header with an alphanumeric name
            size_t idx = 1;
            for(; idx < line.length() - 1; ++idx) {
                if(isalnum(line[idx]) == 0 && line[idx] != '_') {
                    break;
                }
            }
            std::string remain = utils::trim(line.substr(idx + 1));
            if(line[idx] == ']' && (remain.empty() || remain.front() == '#')) {
                // Ignore empty sections if they contain no configurations
                if(!section_name.empty() || conf.size() > 0) {
                    // Add previous section
                    addConfiguration(section_name, std::move(conf));
                }

                // Begin new section
                section_name = std::string(line, 1, idx - 1);
                conf = Configuration();
            } else {
                // Section header is not valid
                throw ConfigParseError(file_name, line_num);
            }
        } else if(isalpha(line.front()) != 0) {
            // Line should be a key / value pair with an equal sign
            try {
                // Parse the key value pair
                auto [key, value] = parseKeyValue(std::move(line));

                // Add the config key
                conf.set(key, value);
            } catch(KeyValueParseError& e) {
                // Rethrow key / value parse error as a configuration parse error
                throw ConfigParseError(file_name, line_num);
            }
        } else {
            // Line is not a comment, key/value pair or section header
            throw ConfigParseError(file_name, line_num);
        }
    }
    // Add last section
    addConfiguration(section_name, std::move(conf));
}

void ConfigParser::addConfiguration(const std::string& name, Configuration config) {
    conf_array_.push_back(std::move(config));

    auto section_name = utils::transform(name, ::tolower);
    conf_map_[section_name].push_back(--conf_array_.end());
}

void ConfigParser::clear() {
    conf_map_.clear();
    conf_array_.clear();
}

bool ConfigParser::hasConfiguration(const std::string& name) const {
    auto conf_name = utils::transform(name, ::tolower);
    return conf_map_.find(conf_name) != conf_map_.end();
}

unsigned int ConfigParser::countConfigurations(const std::string& name) const {
    auto conf_name = utils::transform(name, ::tolower);
    if(!hasConfiguration(conf_name)) {
        return 0;
    }
    return static_cast<unsigned int>(conf_map_.at(name).size());
}

std::vector<Configuration> ConfigParser::getConfigurations(const std::string& name) const {
    auto conf_name = utils::transform(name, ::tolower);
    if(!hasConfiguration(conf_name)) {
        return {};
    }

    std::vector<Configuration> result;
    for(const auto& iter : conf_map_.at(conf_name)) {
        result.push_back(*iter);
    }
    return result;
}

std::vector<Configuration> ConfigParser::getConfigurations() const {
    return {conf_array_.begin(), conf_array_.end()};
}
