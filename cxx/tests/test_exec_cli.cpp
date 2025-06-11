/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/exec/cli.hpp"
#include "constellation/exec/exceptions.hpp"

using namespace Catch::Matchers;
using namespace constellation::exec;
using namespace constellation::log;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Base parser", "[exec]") {
    auto parser = BaseParser("TestProg");
    parser.setup();

    std::vector<const char*> args {"TestProg", "-l", "TRACE"};
    const auto options = parser.parse(args);

    REQUIRE(options.log_level == Level::TRACE);
}

TEST_CASE("Base parser invalid level", "[exec]") {
    auto parser = BaseParser("TestProg");
    parser.setup();

    std::vector<const char*> args {"TestProg", "-l", "ERROR"};
    REQUIRE_THROWS_MATCHES(
        parser.parse(args),
        CommandLineInterfaceError,
        Message("`ERROR` is not a valid log level, possible value are TRACE, DEBUG, INFO, WARNING, STATUS, CRITICAL, OFF"));
}

TEST_CASE("Base parser invalid argument", "[exec]") {
    auto parser = BaseParser("TestProg");
    parser.setup();

    std::vector<const char*> args {"TestProg", "-file", "/tmp/log.txt"};
    REQUIRE_THROWS_MATCHES(parser.parse(args), CommandLineInterfaceError, Message("Unknown argument: -file"));
}

TEST_CASE("Base parser help", "[exec]") {
    auto parser = BaseParser("TestProg");
    parser.setup();

    const auto help = parser.help();
    REQUIRE_THAT(help, ContainsSubstring("Usage: TestProg"));
    REQUIRE_THAT(help, ContainsSubstring("-l, --level"));
    REQUIRE_THAT(help, ContainsSubstring("-i, --interface"));
}

TEST_CASE("Satellite parser (no default type)", "[exec]") {
    auto parser = SatelliteParser("SatelliteTest");
    parser.setup();

    REQUIRE_THAT(parser.help(), ContainsSubstring("-t, --type"));

    std::vector<const char*> args {"SatelliteTest", "-l", "DEBUG", "-g", "edda", "-t", "Test", "-n", "s1"};
    const auto options = parser.parse(args);

    REQUIRE(options.log_level == Level::DEBUG);
    REQUIRE(options.group == "edda");
    REQUIRE(options.satellite_type == "Test");
    REQUIRE(options.satellite_name == "s1");
}

TEST_CASE("Satellite parser (with default type)", "[exec]") {
    auto parser = SatelliteParser("SatelliteTest", {"Test"});
    parser.setup();

    REQUIRE_FALSE(ContainsSubstring("-t, --type").match(parser.help()));

    std::vector<const char*> args {"SatelliteTest", "-l", "INFO", "-g", "edda", "-n", "s1"};
    const auto options = parser.parse(args);

    REQUIRE(options.log_level == Level::INFO);
    REQUIRE(options.group == "edda");
    REQUIRE(options.satellite_type == "Test");
    REQUIRE(options.satellite_name == "s1");
}

TEST_CASE("GUI parser (no group)", "[exec]") {
    auto parser = GUIParser("TestGUI");
    parser.setup();

    std::vector<const char*> args {"TestGUI", "-l", "WARNING", "-n", "lab"};
    const auto options = parser.parse(args);

    REQUIRE(options.log_level == Level::WARNING);
    REQUIRE_FALSE(options.group.has_value());
    REQUIRE(options.instance_name == "lab");
}

TEST_CASE("GUI parser (with group)", "[exec]") {
    auto parser = GUIParser("TestGUI");
    parser.setup();

    std::vector<const char*> args {"TestGUI", "-l", "STATUS", "-g", "edda", "-n", "lab"};
    const auto options = parser.parse(args);

    REQUIRE(options.log_level == Level::STATUS);
    REQUIRE(options.group == "edda");
    REQUIRE(options.instance_name == "lab");
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
