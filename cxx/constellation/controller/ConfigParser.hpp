/**
 * @file
 * @brief Configuration parser class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>

#include <toml++/toml.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::controller {

    /**
     * @brief Generic configuration object storing keys
     *
     * The configuration holds a set of keys with arbitrary values that are internally stored as std::variant.
     */
    class CNSTLN_API ConfigParser {
    public:
        ConfigParser(std::filesystem::path filepath, std::set<std::string> satellites);
        ConfigParser() = delete;
        ~ConfigParser() = default;

        /// @cond doxygen_suppress
        ConfigParser(const ConfigParser& other) = default;
        ConfigParser& operator=(const ConfigParser& other) = default;
        ConfigParser(ConfigParser&& other) noexcept = default;
        ConfigParser& operator=(ConfigParser&& other) = default;
        /// @endcond

        bool hasConfig(const std::string& satellite) const {
            return configs_.contains(utils::transform(satellite, ::tolower));
        }

        config::Dictionary getConfig(const std::string& satellite) const;
        std::map<std::string, config::Dictionary> getAll() { return configs_; }

    private:
        std::optional<config::Value> parse_value(const toml::key& key, auto&& val) const;

        log::Logger logger_;

        std::map<std::string, config::Dictionary> configs_;
    };

} // namespace constellation::controller
