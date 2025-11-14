/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/controller/ControllerConfiguration.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"

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

// --- Creating configuration from code ---

TEST_CASE("Create configuration", "[controller]") {
    Dictionary global_dict {};
    global_dict["level"] = "global";
    global_dict["int"] = 42;
    Dictionary global_subdict {};
    global_subdict["level"] = "global";
    global_subdict["int"] = 42;
    global_dict["dict"] = std::move(global_subdict);
    Dictionary type_dict {};
    type_dict["level"] = "type";
    Dictionary type_subdict {};
    type_subdict["level"] = "type";
    type_dict["dict"] = std::move(type_subdict);
    Dictionary satellite_dict {};
    satellite_dict["level"] = "satellite";
    Dictionary satellite_subdict {};
    satellite_subdict["level"] = "satellite";
    satellite_dict["dict"] = std::move(satellite_subdict);

    ControllerConfiguration config {};
    config.setGlobalConfiguration(std::move(global_dict));
    config.addTypeConfiguration("Dummy", std::move(type_dict));
    config.addSatelliteConfiguration("Dummy.Added", std::move(satellite_dict));
    config.validate();

    // Check global config
    const auto global_config = config.getGlobalConfiguration();
    REQUIRE_THAT(global_config.at("level").get<std::string>(), Equals("global"));
    REQUIRE(global_config.at("int").get<int>() == 42);
    REQUIRE_THAT(global_config.at("dict").get<Dictionary>().at("level").get<std::string>(), Equals("global"));
    REQUIRE(global_config.at("dict").get<Dictionary>().at("int").get<int>() == 42);

    // Check that type config is available
    REQUIRE(config.hasTypeConfiguration("dummy"));
    REQUIRE_FALSE(config.hasTypeConfiguration("dummy2"));

    // Check type config
    const auto type_config = config.getTypeConfiguration("DUMMY");
    REQUIRE_THAT(type_config.at("level").get<std::string>(), Equals("type"));
    REQUIRE(type_config.at("int").get<int>() == 42);
    REQUIRE_THAT(type_config.at("dict").get<Dictionary>().at("level").get<std::string>(), Equals("type"));
    REQUIRE(type_config.at("dict").get<Dictionary>().at("int").get<int>() == 42);

    // Check that satellite config is available
    REQUIRE(config.hasSatelliteConfiguration("dummy.added"));
    REQUIRE_FALSE(config.hasSatelliteConfiguration("dummy2"));

    // Check satellite config
    const auto satellite_config = config.getSatelliteConfiguration("DUMMY.ADDED");
    REQUIRE_THAT(satellite_config.at("level").get<std::string>(), Equals("satellite"));
    REQUIRE(satellite_config.at("int").get<int>() == 42);
    REQUIRE_THAT(satellite_config.at("dict").get<Dictionary>().at("level").get<std::string>(), Equals("satellite"));
    REQUIRE(satellite_config.at("dict").get<Dictionary>().at("int").get<int>() == 42);
}

TEST_CASE("Merge config levels", "[controller]") {
    Dictionary type_dict {};
    type_dict["int"] = 0;
    Dictionary satellite_dict {};
    satellite_dict["int"] = 10;

    ControllerConfiguration config {};
    config.addTypeConfiguration("Dummy", std::move(type_dict));
    config.addSatelliteConfiguration("Dummy.Added", std::move(satellite_dict));

    Dictionary type_dict_updated {};
    type_dict_updated["int"] = 1;
    Dictionary satellite_dict_updated {};
    satellite_dict_updated["int"] = 11;

    config.addTypeConfiguration("dummy", std::move(type_dict_updated));
    config.addSatelliteConfiguration("dummy.added", std::move(satellite_dict_updated));

    REQUIRE(config.getTypeConfiguration("Dummy").at("int").get<int>() == 1);
    REQUIRE(config.getSatelliteConfiguration("Dummy.Added").at("int").get<int>() == 11);
}

TEST_CASE("Merge config with mismatched type", "[controller]") {
    Dictionary type_dict {};
    Dictionary type_subdict {};
    type_subdict["int"] = 0;
    type_dict["dict"] = std::move(type_subdict);
    Dictionary satellite_dict {};
    Dictionary satellite_subdict {};
    satellite_subdict["int"] = Dictionary({{"a", 1}});
    satellite_dict["dict"] = std::move(satellite_subdict);

    ControllerConfiguration config {};
    config.addTypeConfiguration("Dummy", std::move(type_dict));
    config.addSatelliteConfiguration("Dummy.Added", std::move(satellite_dict));

    REQUIRE_THROWS_MATCHES(
        config.getSatelliteConfiguration("Dummy.Added"),
        ConfigValidationError,
        Message("Error validating configuration: value of key `dict.int` has mismatched types when merging defaults"));
}

// --- Invalid YAML ---

TEST_CASE("Invalid YAML", "[controller]") {
    constexpr std::string_view config = "a: b: c";
    REQUIRE_THROWS_AS(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML), ConfigParseError);
}

TEST_CASE("Invalid YAML non-map root node", "[controller]") {
    constexpr std::string_view config = "root_node";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigParseError,
                           Message("Could not parse content of configuration: expected map as root node"));
}

TEST_CASE("Invalid YAML type node not a map", "[controller]") {
    constexpr std::string_view config = "_default: 0";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
        ConfigValueError,
        Message("Error while parsing value of key `_default` in configuration: expected a dictionary at type level"));
}

TEST_CASE("Invalid YAML two global default configs", "[controller]") {
    constexpr std::string_view config = "_default:\n"
                                        "  key: 0\n"
                                        "_DEFAULT:\n"
                                        "  key: 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigKeyError,
                           Message("Error while parsing key `_default` in configuration: key defined twice"));
}

TEST_CASE("Invalid YAML invalid satellite type", "[controller]") {
    constexpr std::string_view config = "satellite-type:\n"
                                        "  satellite-name:\n"
                                        "    key: 0\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigKeyError,
                           Message("Error while parsing key `satellite-type` in configuration: not a valid satellite type"));
}

TEST_CASE("Invalid YAML name node not a map", "[controller]") {
    constexpr std::string_view config = "type:\n"
                                        "  name: 0\n";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
        ConfigValueError,
        Message("Error while parsing value of key `type.name` in configuration: expected a dictionary at satellite level"));
}

TEST_CASE("Invalid YAML two type default configs", "[controller]") {
    constexpr std::string_view config = "type:\n"
                                        "  _default:\n"
                                        "    key: 0\n"
                                        "  _DEFAULT:\n"
                                        "    key: 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigKeyError,
                           Message("Error while parsing key `type._default` in configuration: key defined twice"));
}

TEST_CASE("Invalid YAML invalid satellite name", "[controller]") {
    constexpr std::string_view config = "type:\n"
                                        "  satellite-name:\n"
                                        "    key: 0\n";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
        ConfigKeyError,
        Message("Error while parsing key `type.satellite-name` in configuration: not a valid satellite name"));
}

TEST_CASE("Invalid YAML two satellite configs", "[controller]") {
    constexpr std::string_view config = "type:\n"
                                        "  name:\n"
                                        "    key: 0\n"
                                        "  NAME:\n"
                                        "    key: 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigKeyError,
                           Message("Error while parsing key `type.name` in configuration: key defined twice"));
}

TEST_CASE("Invalid YAML two empty satellite configs", "[controller]") {
    constexpr std::string_view config = "type:\n"
                                        "  name:\n"
                                        "  NAME:\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigKeyError,
                           Message("Error while parsing key `type.name` in configuration: key defined twice"));
}

TEST_CASE("Invalid YAML dict key defined twice", "[controller]") {
    constexpr std::string_view config = "_default:\n"
                                        "  key: 0\n"
                                        "  KEY: 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
                           ConfigKeyError,
                           Message("Error while parsing key `_default.key` in configuration: key defined twice"));
}

TEST_CASE("Invalid YAML inhomogeneous array", "[controller]") {
    constexpr std::string_view config = "_default:\n"
                                        "  array: [ 1, true, 3.14 ]\n";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::YAML),
        ConfigValueError,
        Message("Error while parsing value of key `_default.array` in configuration: array is not homogeneous"));
}

// --- Invalid TOML ---

TEST_CASE("Invalid TOML", "[controller]") {
    constexpr std::string_view config = "a: b: c";
    REQUIRE_THROWS_AS(ControllerConfiguration(config, ControllerConfiguration::FileType::TOML), ConfigParseError);
}

TEST_CASE("Invalid TOML type node not a table", "[controller]") {
    constexpr std::string_view config = "key = 0";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
        ConfigValueError,
        Message("Error while parsing value of key `key` in configuration: expected a dictionary at type level"));
}

TEST_CASE("Invalid TOML two global default configs", "[controller]") {
    constexpr std::string_view config = "[_default]\n"
                                        "key = 0\n"
                                        "[_DEFAULT]\n"
                                        "key = 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
                           ConfigKeyError,
                           Message("Error while parsing key `_default` in configuration: key defined twice"));
}

TEST_CASE("Invalid TOML invalid satellite type", "[controller]") {
    constexpr std::string_view config = "[satellite-type.satellite-name]";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
                           ConfigKeyError,
                           Message("Error while parsing key `satellite-type` in configuration: not a valid satellite type"));
}

TEST_CASE("Invalid TOML name node not a table", "[controller]") {
    constexpr std::string_view config = "[type]\n"
                                        " name = 0\n";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
        ConfigValueError,
        Message("Error while parsing value of key `type.name` in configuration: expected a dictionary at satellite level"));
}

TEST_CASE("Invalid TOML two type default configs", "[controller]") {
    constexpr std::string_view config = "[type._default]\n"
                                        "key = 0\n"
                                        "[TYPE._DEFAULT]\n"
                                        "key = 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
                           ConfigKeyError,
                           Message("Error while parsing key `type._default` in configuration: key defined twice"));
}

TEST_CASE("Invalid TOML invalid satellite name", "[controller]") {
    constexpr std::string_view config = "[type.satellite-name]";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
        ConfigKeyError,
        Message("Error while parsing key `type.satellite-name` in configuration: not a valid satellite name"));
}

TEST_CASE("Invalid TOML two satellite configs", "[controller]") {
    constexpr std::string_view config = "[type.name]\n"
                                        "key = 0\n"
                                        "[type.NAME]\n"
                                        "key = 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
                           ConfigKeyError,
                           Message("Error while parsing key `type.name` in configuration: key defined twice"));
}

TEST_CASE("Invalid TOML dict key defined twice", "[controller]") {
    constexpr std::string_view config = "[_default]\n"
                                        "key = 0\n"
                                        "KEY = 1\n";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
                           ConfigKeyError,
                           Message("Error while parsing key `_default.key` in configuration: key defined twice"));
}

TEST_CASE("Invalid TOML inhomogeneous array", "[controller]") {
    constexpr std::string_view config = "[_default]\n"
                                        "array = [ 1, true, 3.14 ]\n";
    REQUIRE_THROWS_MATCHES(
        ControllerConfiguration(config, ControllerConfiguration::FileType::TOML),
        ConfigValueError,
        Message("Error while parsing value of key `_default.array` in configuration: array is not homogeneous"));
}

// --- File parsing ---

TEST_CASE("Non-existing configuration file", "[controller]") {
    const auto test_file = std::filesystem::path("non-existing.toml");
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(test_file),
                           ConfigFileNotFoundError,
                           Message("Could not read configuration file non-existing.toml"));
}

TEST_CASE("Valid TOML file", "[controller]") {
    const auto test_file = test_files_dir() / "good_config.toml";
    const ControllerConfiguration config {test_file};

    // Global only
    const auto global_config = config.getSatelliteConfiguration("NotA.Satellite");
    REQUIRE(global_config.at("bool").get<bool>() == true);
    REQUIRE(global_config.at("int").get<int>() == -42);
    REQUIRE(global_config.at("float").get<double>() == 3.14);
    REQUIRE(global_config.at("string").get<std::string>() == "global");
    global_config.at("time").get<std::chrono::system_clock::time_point>();
    REQUIRE_THAT(global_config.at("array_bool").get<std::vector<bool>>(),
                 RangeEquals(std::vector<bool>({true, false, false, true})));
    REQUIRE_THAT(global_config.at("array_int").get<std::vector<int>>(), RangeEquals(std::vector<int>({1, 2, 3})));
    REQUIRE_THAT(global_config.at("array_float").get<std::vector<double>>(), RangeEquals(std::vector<double>({0.5, 1.0})));
    REQUIRE_THAT(global_config.at("array_string").get<std::vector<std::string>>(),
                 RangeEquals(std::vector<std::string>({"global1", "global2"})));
    global_config.at("array_time").get<std::vector<std::chrono::system_clock::time_point>>();
    REQUIRE(global_config.at("empty_array").get<std::vector<int>>().empty());
    REQUIRE_THAT(global_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", -1}})));
    REQUIRE(global_config.at("empty_dict").get<Dictionary>().empty());

    // Global + Type
    const auto type_config = config.getSatelliteConfiguration("Dummy.NotASatellite");
    REQUIRE(type_config.at("bool").get<bool>() == true);
    REQUIRE(type_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(type_config.at("string").get<std::string>() == "type");
    REQUIRE_THAT(type_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", 0}})));

    // Global + Type + Satellite
    const auto d1_config = config.getSatelliteConfiguration("Dummy.D1");
    REQUIRE(d1_config.at("bool").get<bool>() == true);
    REQUIRE(d1_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d1_config.at("string").get<std::string>() == "D1");
    REQUIRE(d1_config.at("satellite").get<bool>() == true);
    REQUIRE_THAT(d1_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", 1}})));

    // Case-insensitivity check
    const auto d2_config = config.getSatelliteConfiguration("Dummy.D2");
    REQUIRE(d2_config.at("bool").get<bool>() == true);
    REQUIRE(d2_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d2_config.at("string").get<std::string>() == "D2");
    REQUIRE(d2_config.at("satellite").get<bool>() == true);
    REQUIRE_THAT(d2_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", 2}})));

    // Check that empty satellite configurations are registered
    REQUIRE(config.hasSatelliteConfiguration("Dummy3.D3"));
}

TEST_CASE("Valid YAML file", "[controller]") {
    const auto test_file = test_files_dir() / "good_config.yaml";
    const ControllerConfiguration config {test_file};

    // Global only
    const auto global_config = config.getSatelliteConfiguration("NotA.Satellite");
    REQUIRE(global_config.at("bool").get<bool>() == true);
    REQUIRE(global_config.at("int").get<int>() == -42);
    REQUIRE(global_config.at("float").get<double>() == 3.14);
    REQUIRE(global_config.at("string").get<std::string>() == "global");
    // TODO(stephan.lachnit): check chrono
    REQUIRE_THAT(global_config.at("array_bool").get<std::vector<bool>>(),
                 RangeEquals(std::vector<bool>({true, false, false, true})));
    REQUIRE_THAT(global_config.at("array_int").get<std::vector<int>>(), RangeEquals(std::vector<int>({1, 2, 3})));
    REQUIRE_THAT(global_config.at("array_float").get<std::vector<double>>(), RangeEquals(std::vector<double>({0.5, 1.0})));
    REQUIRE_THAT(global_config.at("array_string").get<std::vector<std::string>>(),
                 RangeEquals(std::vector<std::string>({"global1", "global2"})));
    // TODO(stephan.lachnit): check chrono
    REQUIRE(global_config.at("empty_array").get<std::vector<int>>().empty());
    REQUIRE_THAT(global_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", -1}})));
    REQUIRE(global_config.at("empty_dict").get<Dictionary>().empty());

    // Global + Type
    const auto type_config = config.getSatelliteConfiguration("Dummy.NotASatellite");
    REQUIRE(type_config.at("bool").get<bool>() == true);
    REQUIRE(type_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(type_config.at("string").get<std::string>() == "type");
    REQUIRE_THAT(type_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", 0}})));

    // Global + Type + Satellite
    const auto d1_config = config.getSatelliteConfiguration("Dummy.D1");
    REQUIRE(d1_config.at("bool").get<bool>() == true);
    REQUIRE(d1_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d1_config.at("string").get<std::string>() == "D1");
    REQUIRE(d1_config.at("satellite").get<bool>() == true);
    REQUIRE_THAT(d1_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", 1}})));

    // Case-insensitivity check
    const auto d2_config = config.getSatelliteConfiguration("Dummy.D2");
    REQUIRE(d2_config.at("bool").get<bool>() == true);
    REQUIRE(d2_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d2_config.at("string").get<std::string>() == "D2");
    REQUIRE(d2_config.at("satellite").get<bool>() == true);
    REQUIRE_THAT(d2_config.at("dict").get<Dictionary>().at("subdict").get<Dictionary>().getMap<int>(),
                 RangeEquals(std::map<std::string, int>({{"key", 2}})));

    // Check that empty satellite configurations are registered
    REQUIRE(config.hasSatelliteConfiguration("Dummy3.D3"));
}

// --- Configuration emitting ---

TEST_CASE("Get as TOML", "[controller]") {
    const std::chrono::system_clock::time_point time_now_floored =
        std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());

    Dictionary global_dict {};
    global_dict["level"] = "global";
    global_dict["int"] = -42;
    global_dict["time"] = time_now_floored;
    global_dict["array_bool"] = Array({true, false, false, true});
    global_dict["array_int"] = Array({1, 2, 3});
    global_dict["empty_array"] = Array();
    global_dict["dict"] = Dictionary({{"a", 1}, {"b", 2}});
    Dictionary type_dict {};
    type_dict["level"] = "type";
    Dictionary satellite_dict {};
    satellite_dict["level"] = "satellite";

    ControllerConfiguration config {};
    config.setGlobalConfiguration(std::move(global_dict));
    config.addTypeConfiguration("dummy", std::move(type_dict));
    config.addSatelliteConfiguration("Dummy.Added", std::move(satellite_dict));
    config.validate();

    const auto toml = config.getAsTOML();
    const ControllerConfiguration config2 {toml, ControllerConfiguration::FileType::TOML};

    REQUIRE_THAT(config.getGlobalConfiguration(), UnorderedRangeEquals(config2.getGlobalConfiguration()));
    REQUIRE_THAT(config.getTypeConfiguration("Dummy"), UnorderedRangeEquals(config2.getTypeConfiguration("Dummy")));
    REQUIRE_THAT(config.getSatelliteConfiguration("dummy.added"),
                 UnorderedRangeEquals(config2.getSatelliteConfiguration("Dummy.Added")));
}

TEST_CASE("Get as YAML", "[controller]") {
    Dictionary global_dict {};
    global_dict["level"] = "global";
    global_dict["int"] = -42;
    global_dict["array_bool"] = Array({true, false, false, true});
    global_dict["array_int"] = Array({1, 2, 3});
    global_dict["empty_array"] = Array();
    global_dict["dict"] = Dictionary({{"a", 1}, {"b", 2}});
    Dictionary type_dict {};
    type_dict["level"] = "type";
    Dictionary satellite_dict {};
    satellite_dict["level"] = "satellite";

    ControllerConfiguration config {};
    config.setGlobalConfiguration(std::move(global_dict));
    config.addTypeConfiguration("dummy", std::move(type_dict));
    config.addSatelliteConfiguration("Dummy.Added", std::move(satellite_dict));
    config.validate();

    const auto toml = config.getAsYAML();
    const ControllerConfiguration config2 {toml, ControllerConfiguration::FileType::YAML};

    REQUIRE_THAT(config.getGlobalConfiguration(), UnorderedRangeEquals(config2.getGlobalConfiguration()));
    REQUIRE_THAT(config.getTypeConfiguration("Dummy"), UnorderedRangeEquals(config2.getTypeConfiguration("Dummy")));
    REQUIRE_THAT(config.getSatelliteConfiguration("dummy.added"),
                 UnorderedRangeEquals(config2.getSatelliteConfiguration("Dummy.Added")));
}

// --- Validation ---

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
                           ConfigValidationError,
                           Message("Error validating configuration: Cyclic dependency for transition `starting`"));
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
                           ConfigValidationError,
                           Message("Error validating configuration: Cyclic dependency for transition `launching`"));
}

TEST_CASE("Self-dependency", "[controller]") {
    ControllerConfiguration config;
    Dictionary dict_sat_a;
    dict_sat_a["_require_starting_after"] = "dummy.a";

    config.addSatelliteConfiguration("dummy.a", dict_sat_a);
    REQUIRE(config.hasSatelliteConfiguration("dummy.a"));

    // No exception thrown
    REQUIRE_THROWS_MATCHES(config.validate(),
                           ConfigValidationError,
                           Message("Error validating configuration: Cyclic dependency for transition `starting`"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
