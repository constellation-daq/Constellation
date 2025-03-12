/**
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <catch2/catch_test_macros.hpp>

#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/ManagerRegistry.hpp"

#include "chirp_mock.hpp"

using namespace constellation::log;
using namespace constellation::utils;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Delayed first message", "[logging]") {
    // First message is delayed by 500ms, so call this here for better timing analysis
    ManagerRegistry::getSinkManager().updateCMDPLevels(TRACE);
    ManagerRegistry::getSinkManager().setConsoleLevels(OFF);
    auto logger = Logger("DelayedFirstMessage");
    LOG(logger, TRACE) << "";
    ManagerRegistry::getSinkManager().updateCMDPLevels(OFF);
}

TEST_CASE("Default logger", "[logging]") {
    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);
    LOG(Logger::getDefault(), STATUS) << "Message from default logger";
    // Default logger is not destructed and thus requires manual flushing
    Logger::getDefault().flush();
}

TEST_CASE("Basic logging", "[logging]") {
    auto logger = Logger("BasicLogging");

    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);
    REQUIRE(logger.shouldLog(TRACE));

    LOG(logger, TRACE) << "trace";
    LOG(logger, DEBUG) << "debug";
    LOG(logger, INFO) << "info";
    LOG(logger, STATUS) << "status";
    LOG(logger, WARNING) << "warning";
    LOG(logger, CRITICAL) << "critical";
}

TEST_CASE("Logging with default logger", "[logging]") {

    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);

    LOG(TRACE) << "trace";
    LOG(DEBUG) << "debug";
    LOG(INFO) << "info";
    LOG(STATUS) << "status";
    LOG(WARNING) << "warning";
    LOG(CRITICAL) << "critical";
}

TEST_CASE("Logging from const function", "[logging]") {

    class LogTest {
    public:
        void log() const { LOG(logger_, CRITICAL) << "const critical"; }

    private:
        Logger logger_ {"ConstLogging"};
    };

    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);
    const LogTest log_test {};
    log_test.log();
}

TEST_CASE("Logging macros", "[logging]") {
    auto logger = Logger("LoggingMacros");

    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);

    int count_once {0};
    int count_n {0};
    int count_if {0};
    int count_nth {0};

    for(int i = 0; i < 5; ++i) {
        LOG_ONCE(logger, STATUS) << "log once, i=" << i << ", count " << ++count_once;
        LOG_N(logger, STATUS, 3) << "log n, i=" << i << ", count " << ++count_n;
        LOG_IF(logger, STATUS, i % 2 == 1) << "log if, i=" << i << ", count " << ++count_if;
        LOG_NTH(logger, STATUS, 2) << "log_nth, i=" << i << ", count " << ++count_nth;
    }

    REQUIRE(count_once == 1);
    REQUIRE(count_n == 3);
    REQUIRE(count_if == 2);
    REQUIRE(count_nth == 3);
}

TEST_CASE("Logging macros with default logger", "[logging]") {

    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);

    int count_once {0};
    int count_n {0};
    int count_if {0};

    for(int i = 0; i < 5; ++i) {
        LOG_ONCE(STATUS) << "log once, i=" << i << ", count " << ++count_once;
        LOG_N(STATUS, 3) << "log n, i=" << i << ", count " << ++count_n;
        LOG_IF(STATUS, i % 2 == 1) << "log if, i=" << i << ", count " << ++count_if;
    }

    REQUIRE(count_once == 1);
    REQUIRE(count_n == 3);
    REQUIRE(count_if == 2);
}

TEST_CASE("Log levels", "[logging]") {
    auto logger = Logger("LogLevels");

    ManagerRegistry::getSinkManager().setConsoleLevels(STATUS);
    ManagerRegistry::getSinkManager().updateCMDPLevels(STATUS);
    REQUIRE(logger.getLogLevel() == STATUS);

    // Test global CMDP subscription
    ManagerRegistry::getSinkManager().updateCMDPLevels(DEBUG);
    REQUIRE(logger.getLogLevel() == DEBUG);

    // Test global CMDP unsubscription
    ManagerRegistry::getSinkManager().updateCMDPLevels(OFF);
    REQUIRE(logger.getLogLevel() == STATUS);

    // Test topic CMDP subscription - topics are uppercase
    ManagerRegistry::getSinkManager().updateCMDPLevels(STATUS, {{"LOGLEVELS", DEBUG}});
    REQUIRE(logger.getLogLevel() == DEBUG);

    // Test topic CMDP subscription via matching - topics are uppercase
    ManagerRegistry::getSinkManager().updateCMDPLevels(STATUS, {{"LOGLEVELS", DEBUG}, {"LOGLE", TRACE}});
    REQUIRE(logger.getLogLevel() == TRACE);

    // Test higher CMDP topic level higher than global does not lower logger level
    ManagerRegistry::getSinkManager().updateCMDPLevels(DEBUG, {{"LOGLEVELS", INFO}});
    REQUIRE(logger.getLogLevel() == DEBUG);

    // Test higher console level than CMDP level does not lower global level
    ManagerRegistry::getSinkManager().setConsoleLevels(WARNING, {{"LOGLEVELS", CRITICAL}});
    REQUIRE(logger.getLogLevel() == DEBUG);

    // Test global console level
    ManagerRegistry::getSinkManager().updateCMDPLevels(OFF, {{"LOGLEVELS", CRITICAL}});
    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE);
    REQUIRE(logger.getLogLevel() == TRACE);

    // Test topic console level overwrites global console level
    ManagerRegistry::getSinkManager().setConsoleLevels(TRACE, {{"LOGLEVELS", WARNING}});
    REQUIRE(logger.getLogLevel() == WARNING);
}

TEST_CASE("Ephemeral CMDP port", "[logging]") {
    // Port number of ephemeral port should always be >=1024 on all OSes
    auto port_number = ManagerRegistry::getSinkManager().getCMDPPort();
    REQUIRE(port_number >= 1024);
}

TEST_CASE("Register Service via CHIRP", "[logging]") {
    auto* manager = create_chirp_manager();
    ManagerRegistry::getSinkManager().enableCMDPSending("satname");
    REQUIRE(manager->getRegisteredServices().size() == 1);
    using namespace constellation::protocol::CHIRP;
    REQUIRE(manager->getRegisteredServices().contains({MONITORING, ManagerRegistry::getSinkManager().getCMDPPort()}));
    manager->forgetDiscoveredServices();
}

// TODO(stephan.lachnit): test log message decoding

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
