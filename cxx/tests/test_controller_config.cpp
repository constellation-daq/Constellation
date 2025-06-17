/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/controller/ControllerConfiguration.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/Dictionary.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::controller;

namespace {
    std::filesystem::path test_files_dir() {
        const auto* cxx_tests_dir = std::getenv("CXX_TESTS_DIR"); // NOLINT(concurrency-mt-unsafe)
        if(cxx_tests_dir == nullptr) {
            return std::filesystem::current_path() / "cxx" / "tests" / "test_files";
        }
        return std::filesystem::path(cxx_tests_dir) / "test_files";
    }
} // namespace

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Non-existing TOML file", "[controller]") {
    const auto test_file = std::filesystem::path("non-existing.toml");
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(test_file),
                           ConfigFileNotFoundError,
                           Message("Could not read configuration file non-existing.toml"));
}

TEST_CASE("Invalid TOML file", "[controller]") {
    const auto test_file = test_files_dir() / "invalid_toml.txt";
    REQUIRE_THROWS_AS(ControllerConfiguration(test_file), ConfigFileParseError);
}

TEST_CASE("Inhomogeneous array", "[controller]") {
    const auto test_file = test_files_dir() / "inhomogeneous_array.toml";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(test_file),
                           ConfigFileTypeError,
                           Message("Invalid value type for key array: Array is not homogeneous"));
}

TEST_CASE("Valid TOML file", "[controller]") {
    const auto test_file = test_files_dir() / "good_config.toml";
    const ControllerConfiguration config {test_file};

    // Global only
    const auto global_config = config.getSatelliteConfiguration("NotA.Satellite");
    REQUIRE(global_config.at("bool").get<bool>() == true);
    REQUIRE(global_config.at("string").get<std::string>() == "global");
    REQUIRE(global_config.at("int").get<int>() == -42);
    REQUIRE(global_config.at("float").get<double>() == 3.14);
    REQUIRE(global_config.at("array_bool").get<std::vector<bool>>() == std::vector<bool>({true, false, false, true}));
    REQUIRE(global_config.at("array_string").get<std::vector<std::string>>() ==
            std::vector<std::string>({"global1", "global2"}));
    REQUIRE(global_config.at("array_int").get<std::vector<int>>() == std::vector<int>({1, 2, 3}));
    REQUIRE(global_config.at("array_float").get<std::vector<double>>() == std::vector<double>({0.1}));
    REQUIRE(std::holds_alternative<std::monostate>(global_config.at("empty_array")));

    // Global + Type
    const auto section_config = config.getSatelliteConfiguration("Dummy.NotASatellite");
    REQUIRE(section_config.at("bool").get<bool>() == true);
    REQUIRE(section_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(section_config.at("string").get<std::string>() == "type");

    // Global + Type + Satellite
    const auto d1_config = config.getSatelliteConfiguration("Dummy.D1");
    REQUIRE(d1_config.at("bool").get<bool>() == true);
    REQUIRE(d1_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d1_config.at("string").get<std::string>() == "D1");
    REQUIRE(d1_config.at("satellite").get<bool>() == true);

    // Case-insensitivity check
    const auto d2_config = config.getSatelliteConfiguration("Dummy.D2");
    REQUIRE(d2_config.at("bool").get<bool>() == true);
    REQUIRE(d2_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d2_config.at("string").get<std::string>() == "D2");
    REQUIRE(d2_config.at("satellite").get<bool>() == true);

    // Check if config is available
    REQUIRE(config.hasSatelliteConfiguration("Dummy.D1"));
    REQUIRE(config.hasSatelliteConfiguration("duMMy.d1"));
}

TEST_CASE("No global section", "[controller]") {
    const std::string_view toml_string = "# Config without global section";
    const ControllerConfiguration config {toml_string};
    const auto global_config = config.getSatelliteConfiguration("NotA.Satellite");
    REQUIRE(global_config.empty());
}

TEST_CASE("Adding satellite configuration", "[controller]") {
    ControllerConfiguration config;
    Dictionary dict;
    dict["key"] = 13;

    config.addSatelliteConfiguration("Dummy.Added", dict);
    REQUIRE(config.hasSatelliteConfiguration("Dummy.Added"));

    const auto satellite_config = config.getSatelliteConfiguration("Dummy.Added");
    REQUIRE(satellite_config.at("key").get<int>() == 13);
}

TEST_CASE("Convert to TOML", "[controller]") {
    ControllerConfiguration config;
    Dictionary dict;
    dict["bool"] = true;
    dict["int"] = 123;
    dict["double"] = 1.0;
    dict["str"] = "Hello World!";
    dict["bool_arr"] = std::vector<bool>({true, false});
    dict["int_arr"] = std::vector<int>({1, 2, 3});
    dict["double_arr"] = std::vector<double>({1.0});
    dict["str_arr"] = std::vector<std::string>({"Hello", "World"});

    config.addSatelliteConfiguration("Dummy.Added", dict);
    REQUIRE(config.hasSatelliteConfiguration("Dummy.Added"));

    // Get as TOML
    const auto toml = config.getAsTOML();
    REQUIRE_THAT(toml, ContainsSubstring("[satellites.dummy.added]"));
    REQUIRE_THAT(toml, ContainsSubstring("bool = true"));
    REQUIRE_THAT(toml, ContainsSubstring("int = 123"));
    REQUIRE_THAT(toml, ContainsSubstring("double = 1.0"));
    REQUIRE_THAT(toml, ContainsSubstring("str = 'Hello World!'"));
    REQUIRE_THAT(toml, ContainsSubstring("bool_arr = [ true, false ]"));
    REQUIRE_THAT(toml, ContainsSubstring("int_arr = [ 1, 2, 3 ]"));
    REQUIRE_THAT(toml, ContainsSubstring("double_arr = [ 1.0 ]"));
    REQUIRE_THAT(toml, ContainsSubstring("str_arr = [ 'Hello', 'World' ]"));

    // Parse TOML
    const ControllerConfiguration config2 {std::string_view(toml)};
    const auto dict2 = config.getSatelliteConfiguration("Dummy.Added");
    REQUIRE(dict2.at("bool").get<bool>() == true);
    REQUIRE(dict2.at("int").get<int>() == 123);
    REQUIRE(dict2.at("double").get<double>() == 1.0);
    REQUIRE(dict2.at("str").get<std::string>() == "Hello World!");
    REQUIRE(dict2.at("bool_arr").get<std::vector<bool>>() == std::vector<bool>({true, false}));
    REQUIRE(dict2.at("int_arr").get<std::vector<int>>() == std::vector<int>({1, 2, 3}));
    REQUIRE(dict2.at("double_arr").get<std::vector<double>>() == std::vector<double>({1.0}));
    REQUIRE(dict2.at("str_arr").get<std::vector<std::string>>() == std::vector<std::string>({"Hello", "World"}));
}

TEST_CASE("Valid dependency graph", "[controller]") {
    ControllerConfiguration config;

    // A depends on B
    Dictionary dict_sat_a;
    dict_sat_a["_require_starting_after"] = "dummy.b";

    config.addSatelliteConfiguration("dummy.a", dict_sat_a);
    config.addSatelliteConfiguration("dummy.b", {});
    REQUIRE(config.hasSatelliteConfiguration("dummy.a"));
    REQUIRE(config.hasSatelliteConfiguration("dummy.b"));

    // No exception thrown
    config.validate();

    // A depends on B and C,
    // C depends on B
    dict_sat_a["_require_starting_after"] = std::vector<std::string> {"dummy.b", "dummy.c"};
    Dictionary dict_sat_c;
    dict_sat_c["_require_starting_after"] = "dummy.b";

    config.addSatelliteConfiguration("dummy.a", dict_sat_a);
    config.addSatelliteConfiguration("dummy.c", dict_sat_c);
    REQUIRE(config.hasSatelliteConfiguration("dummy.a"));
    REQUIRE(config.hasSatelliteConfiguration("dummy.b"));
    REQUIRE(config.hasSatelliteConfiguration("dummy.c"));

    // No exception thrown
    config.validate();
}

TEST_CASE("Direct cyclic dependency graph", "[controller]") {
    ControllerConfiguration config;
    Dictionary dict_sat_a;
    dict_sat_a["_require_starting_after"] = "dummy.b";
    Dictionary dict_sat_b;
    dict_sat_b["_require_starting_after"] = "dummy.a";

    config.addSatelliteConfiguration("dummy.a", dict_sat_a);
    config.addSatelliteConfiguration("dummy.b", dict_sat_b);
    REQUIRE(config.hasSatelliteConfiguration("dummy.a"));
    REQUIRE(config.hasSatelliteConfiguration("dummy.b"));

    // No exception thrown
    REQUIRE_THROWS_MATCHES(config.validate(),
                           ConfigFileValidationError,
                           Message("Error validating configuration file: Cyclic dependency for transition `starting`"));
}

TEST_CASE("Indirect cyclic dependency graph", "[controller]") {
    ControllerConfiguration config;
    Dictionary dict_sat_a;
    dict_sat_a["_require_launching_after"] = "dummy.b";
    Dictionary dict_sat_b;
    dict_sat_b["_require_launching_after"] = "dummy.c";
    Dictionary dict_sat_c;
    dict_sat_c["_require_launching_after"] = "dummy.a";

    config.addSatelliteConfiguration("dummy.a", dict_sat_a);
    config.addSatelliteConfiguration("dummy.b", dict_sat_b);
    config.addSatelliteConfiguration("dummy.c", dict_sat_c);
    REQUIRE(config.hasSatelliteConfiguration("dummy.a"));
    REQUIRE(config.hasSatelliteConfiguration("dummy.b"));
    REQUIRE(config.hasSatelliteConfiguration("dummy.c"));

    // No exception thrown
    REQUIRE_THROWS_MATCHES(config.validate(),
                           ConfigFileValidationError,
                           Message("Error validating configuration file: Cyclic dependency for transition `launching`"));
}

TEST_CASE("Self-dependency", "[controller]") {
    ControllerConfiguration config;
    Dictionary dict_sat_a;
    dict_sat_a["_require_starting_after"] = "dummy.a";

    config.addSatelliteConfiguration("dummy.a", dict_sat_a);
    REQUIRE(config.hasSatelliteConfiguration("dummy.a"));

    // No exception thrown
    REQUIRE_THROWS_MATCHES(config.validate(),
                           ConfigFileValidationError,
                           Message("Error validating configuration file: Cyclic dependency for transition `starting`"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
