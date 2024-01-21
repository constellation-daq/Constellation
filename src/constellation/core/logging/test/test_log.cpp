/**
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/logging/SinkManager.hpp"

using namespace constellation::log;
using namespace std::literals::chrono_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

#define LOGGER logger

TEST_CASE("Create sink manager", "[logging]") {
    // Used to create sink manager in separate test for better timing analysis
    SinkManager::getInstance();
}

TEST_CASE("Basic logging", "[logging]") {
    auto logger = constellation::log::Logger("BasicLogging");

    SinkManager::getInstance().setGlobalConsoleLevel(TRACE);
    REQUIRE(SHOULD_LOG(TRACE));

    LOG(TRACE) << "trace"sv;
    LOG(DEBUG) << "debug"sv;
    LOG(INFO) << "info"sv;
    LOG(STATUS) << "status"sv;
    LOG(WARNING) << "warning"sv;
    LOG(CRITICAL) << "critical"sv;

    // Wait for logging to be flushed for proper output with Catch2
    logger.flush();
    std::this_thread::sleep_for(1ms);
}

TEST_CASE("Logging macros", "[logging]") {
    auto logger = constellation::log::Logger("LoggingMacros");

    SinkManager::getInstance().setGlobalConsoleLevel(TRACE);

    int count_once {0};
    int count_n {0};
    int count_if {0};

    for(int i = 0; i < 5; ++i) {
        LOG_ONCE(STATUS) << "log once, i="sv << i << ", count "sv << ++count_once;
        LOG_N(STATUS, 3) << "log n, i="sv << i << ", count "sv << ++count_n;
        LOG_IF(STATUS, i % 2 == 1) << "log if, i=" << i << ", count "sv << ++count_if;
    }

    REQUIRE(count_once == 1);
    REQUIRE(count_n == 3);
    REQUIRE(count_if == 2);

    // Wait for logging to be flushed for proper output with Catch2
    logger.flush();
    std::this_thread::sleep_for(1ms);
}

TEST_CASE("Log levels", "[logging]") {
    auto logger = constellation::log::Logger("LogLevels", INFO);

    SinkManager::getInstance().setGlobalConsoleLevel(STATUS);
    SinkManager::getInstance().setCMDPLevelsCustom(STATUS);

    REQUIRE_FALSE(logger.shouldLog(DEBUG));
    REQUIRE(logger.shouldLog(INFO));

    // Test global CMDP subscription
    SinkManager::getInstance().setCMDPLevelsCustom(DEBUG);
    REQUIRE(logger.shouldLog(DEBUG));

    // Test global CMDP unsubscription
    SinkManager::getInstance().setCMDPLevelsCustom(OFF);
    REQUIRE_FALSE(logger.shouldLog(DEBUG));

    // Test topic CMDP subscription
    SinkManager::getInstance().setCMDPLevelsCustom(STATUS, {{"LogLevels", DEBUG}});
    REQUIRE(logger.shouldLog(DEBUG));

    // Test topic CMDP subscription via matching
    SinkManager::getInstance().setCMDPLevelsCustom(STATUS, {{"LogLevels", DEBUG}, {"logle", TRACE}});
    REQUIRE(logger.shouldLog(TRACE));
}

TEST_CASE("Ephemeral CMDP1 port", "[logging]") {
    // Port number of ephemeral port should always be >=1024 on all OSes
    auto port_number = SinkManager::getInstance().getCMDP1Port();
    REQUIRE(port_number >= 1024);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
