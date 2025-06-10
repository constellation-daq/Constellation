/**
 * @file
 * @brief Implementation of the main function for a satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "satellite.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <argparse/argparse.hpp>
#include <asio.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation;
using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::networking;
using namespace constellation::satellite;
using namespace constellation::utils;

namespace {
    // Use global std::function to work around C linkage
    std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

extern "C" void signal_handler(int signal) {
    signal_handler_f(signal);
}

namespace {
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser, bool needs_type) {
        // If not a predefined type, requires that the satellite type is specified
        if(needs_type) {
            parser.add_argument("-t", "--type").help("satellite type").required();
        }

        // Satellite name (-n)
        // Note: canonical satellite name = type_name.satellite_name
        try {
            // Try to use host name as default, replace hyphens with underscores:
            auto default_name = asio::ip::host_name();
            std::ranges::replace(default_name, '-', '_');
            parser.add_argument("-n", "--name").help("satellite name").default_value(default_name);
        } catch(const asio::system_error& error) {
            parser.add_argument("-n", "--name").help("satellite name").required();
        }

        // Constellation group (-g)
        parser.add_argument("-g", "--group").help("group name").required();

        // Console log level (-l)
        parser.add_argument("-l", "--level").help("log level").default_value("INFO");

        // TODO(stephan.lachnit): module specific console log level

        // Interface address (--if)
        parser.add_argument("--if").help("interface address");

        // Note: this might throw
        parser.parse_args(argc, argv);
    }

    // parser.get() might throw a logic error, but this never happens in practice
    std::string get_arg(argparse::ArgumentParser& parser, std::string_view arg) noexcept {
        try {
            return parser.get(arg);
        } catch(const std::exception&) {
            std::unreachable();
        }
    }
} // namespace

int constellation::exec::satellite_main(int argc,
                                        char* argv[], // NOLINT(modernize-avoid-c-arrays)
                                        std::string_view program,
                                        std::optional<SatelliteType> satellite_type) noexcept {
    // Ensure that ZeroMQ doesn't fail creating the CMDP sink
    try {
        ManagerLocator::getInstance();
    } catch(const NetworkError& error) {
        std::cerr << "Failed to initialize logging: " << error.what() << "\n" << std::flush;
        return 1;
    }

    // Get the default logger
    auto& logger = Logger::getDefault();

    // If we need to parse the type name via CLI
    const auto needs_type = !satellite_type.has_value();

    // CLI parsing
    argparse::ArgumentParser parser {to_string(program), CNSTLN_VERSION_FULL};
    try {
        parse_args(argc, argv, parser, needs_type);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \"" << program << " --help\" for help";
        return 1;
    }

    // Set log level
    const auto default_level = enum_cast<Level>(get_arg(parser, "level"));
    if(!default_level.has_value()) {
        LOG(logger, CRITICAL) << "Log level \"" << get_arg(parser, "level") << "\" is not valid"
                              << ", possible values are: " << utils::list_enum_names<Level>();
        return 1;
    }
    ManagerLocator::getSinkManager().setConsoleLevels(default_level.value());

    // Check interface address
    std::optional<asio::ip::address_v4> if_addr {};
    try {
        const auto if_string = parser.present("if");
        if(if_string.has_value()) {
            if_addr = asio::ip::make_address_v4(if_string.value());
        }
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid interface address \"" << get_arg(parser, "if") << "\"";
        return 1;
    } catch(const std::exception&) {
        std::unreachable();
    }

    // Get satellite type and name
    auto type_name = needs_type ? get_arg(parser, "type") : std::move(satellite_type.value().type_name);
    const auto satellite_name = get_arg(parser, "name");

    // Get group name
    const auto group_name = parser.get("group");

    // Log the version after all the basic checks are done
    LOG(logger, STATUS) << "Constellation " << CNSTLN_VERSION_FULL;

    // Load satellite DSO
    std::unique_ptr<DSOLoader> loader {};
    Generator* satellite_generator {};
    try {
        loader = needs_type ? std::make_unique<DSOLoader>(type_name, logger)
                            : std::make_unique<DSOLoader>(type_name, logger, satellite_type.value().dso_path);
        satellite_generator = loader->loadSatelliteGenerator();
    } catch(const DSOLoaderError& error) {
        LOG(logger, CRITICAL) << "Error loading satellite type \"" << type_name << "\": " << error.what();
        return 1;
    }

    // Use properly capitalized satellite type for the canonical name:
    type_name = loader->getDSOName();
    const auto canonical_name = type_name + "." + satellite_name;

    // Create CHIRP manager and set as default
    std::unique_ptr<chirp::Manager> chirp_manager {};
    try {
        if(if_addr.has_value()) {
            chirp_manager = std::make_unique<chirp::Manager>(group_name, canonical_name, if_addr.value());
        } else {
            chirp_manager = std::make_unique<chirp::Manager>(group_name, canonical_name);
        }
        chirp_manager->start();
        ManagerLocator::setDefaultCHIRPManager(std::move(chirp_manager));
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
        // TODO(stephan.lachnit): should we continue anyway or abort?
    }

    // Register CMDP in CHIRP and set sender name for CMDP
    ManagerLocator::getSinkManager().enableCMDPSending(canonical_name);

    // Create satellite
    LOG(logger, STATUS) << "Starting satellite " << canonical_name;
    std::shared_ptr<Satellite> satellite {};
    try {
        satellite = satellite_generator(type_name, satellite_name);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to create satellite: " << error.what();
        return 1;
    }

    // Register signal handlers
    std::once_flag shut_down_flag {};
    signal_handler_f = [&](int /*signal*/) -> void {
        std::call_once(shut_down_flag, [&]() {
            LOG(logger, STATUS) << "Terminating satellite";
            satellite->terminate();
        });
    };
    // NOLINTBEGIN(cert-err33-c)
    std::signal(SIGTERM, &signal_handler);
    std::signal(SIGINT, &signal_handler);
    // NOLINTEND(cert-err33-c)

    // Wait for signal to join
    satellite->join();

    // Unregister callbacks
    ManagerLocator::getCHIRPManager()->unregisterDiscoverCallbacks();

    return 0;
}
