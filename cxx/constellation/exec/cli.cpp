/**
 * @file
 * @brief Implementation of command-line interface parser
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "cli.hpp"

#include <algorithm>
#include <exception>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <argparse/argparse.hpp>
#include <asio.hpp>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/exceptions.hpp"

using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::networking;
using namespace constellation::utils;

BaseParser::BaseParser(std::string program) : argparse::ArgumentParser(std::move(program), CNSTLN_VERSION_FULL) {}

void BaseParser::setup() {
    // Console log level (-l)
    add_argument("-l", "--level").help("log level").default_value("INFO");

    // TODO(stephan.lachnit): module specific console log level

    // Interfaces (-i)
    try {
        // Get interface names
        const auto interfaces = get_interfaces();
        std::vector<std::string> interface_names {};
        interface_names.reserve(interfaces.size());
        std::ranges::transform(
            interfaces, std::back_inserter(interface_names), [](const auto& interface) { return interface.name; });

        add_argument("-i", "--interface").help("network interface").append().default_value(interface_names);
    } catch(const std::exception& error) {
        add_argument("-i", "--interface").help("network interface").append().required();
    }
}

BaseParser::BaseOptions BaseParser::parse(std::span<const char*> args) {
    // Parse args
    try {
        parse_args(static_cast<int>(args.size()), args.data());
    } catch(const std::runtime_error& error) {
        throw CommandLineInterfaceError(error.what());
    }

    // Get log level
    const auto level_str = get("level");
    const auto level = enum_cast<Level>(level_str);
    if(!level.has_value()) {
        throw CommandLineInterfaceError("`" + level_str + "` is not a valid log level, possible value are " +
                                        list_enum_names<Level>());
    }

    // Get interfaces
    const auto interface_names = get<std::vector<std::string>>("interface");
    const auto interfaces = get_interfaces(interface_names);

    return {level.value(), interfaces};
}

std::string BaseParser::help() const {
    return argparse::ArgumentParser::help().str();
}

SatelliteParser::SatelliteParser(std::string program, std::optional<std::string> type)
    : BaseParser(std::move(program)), type_(std::move(type)) {}

void SatelliteParser::setup() {
    // If not a predefined type, require that the satellite type is specified
    if(!type_.has_value()) {
        add_argument("-t", "--type").help("satellite type").required();
    }

    // Constellation group (-g)
    add_argument("-g", "--group").help("group name").required();

    // Satellite name (-n)
    try {
        // Try to use host name as default
        const auto default_name = get_hostname();
        add_argument("-n", "--name").help("satellite name").default_value(default_name);
    } catch(const asio::system_error& error) {
        add_argument("-n", "--name").help("satellite name").required();
    }

    // Add base options
    BaseParser::setup();
}

SatelliteParser::SatelliteOptions SatelliteParser::parse(std::span<const char*> args) {
    // Parse base args
    const auto base_options = BaseParser::parse(args);

    // Get group
    const auto group = get("group");

    // Get satellite type
    const auto type = type_.has_value() ? type_.value() : get("type");

    // Get satellite name
    const auto name = get("name");

    return {base_options, group, type, name};
}

GUIParser::GUIParser(std::string program) : BaseParser(std::move(program)) {}

void GUIParser::setup() {
    // Constellation group (-g)
    add_argument("-g", "--group").help("group name");

    // Instance name (-n)
    try {
        // Try to use host name as default
        const auto default_name = get_hostname();
        add_argument("-n", "--name").help("instance name").default_value(default_name);
    } catch(const asio::system_error& error) {
        add_argument("-n", "--name").help("instance name");
    }

    // Add base options
    BaseParser::setup();
}

GUIParser::GUIOptions GUIParser::parse(std::span<const char*> args) {
    // Parse base args
    const auto base_options = BaseParser::parse(args);

    // Get group
    const auto group = present("group");

    // Get instance name
    std::optional<std::string> name;
    try {
        name = get("name");
    } catch(const std::logic_error&) { // NOLINT(bugprone-empty-catch)
    }

    return {base_options, group, std::move(name)};
}
