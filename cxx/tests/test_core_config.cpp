/**
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/utils/type.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::utils;

enum class Enum : std::uint8_t { A, B };

namespace {
    std::filesystem::path test_files_dir() {
        const auto* cxx_tests_dir = std::getenv("CXX_TESTS_DIR"); // NOLINT(concurrency-mt-unsafe)
        if(cxx_tests_dir == nullptr) {
            return std::filesystem::current_path() / "cxx" / "tests" / "test_files";
        }
        return std::filesystem::path(cxx_tests_dir) / "test_files";
    }
} // namespace

namespace {
    enum class MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
} // namespace

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Configuration constructors and operators", "[core][core::config]") {
    const Configuration config_empty {};
    REQUIRE(config_empty.empty());
    Dictionary dict {};
    dict["test"] = true;
    Configuration config_dict {dict};
    REQUIRE(config_dict.asDictionary() == dict);
    Configuration config_moved {std::move(config_dict)};
    REQUIRE(config_moved.asDictionary() == dict);
    REQUIRE(config_dict.empty()); // NOLINT(bugprone-use-after-move)
    Configuration config_assigned {};
    REQUIRE(config_assigned.empty());
    config_assigned = std::move(config_moved);
    REQUIRE(config_assigned.asDictionary() == dict);
    REQUIRE(config_moved.empty()); // NOLINT(bugprone-use-after-move)
}

TEST_CASE("Configuration has and count", "[core][core::config]") {
    Dictionary dict {};
    dict["output_active"] = true;
    dict["fixed_voltage"] = 5.0;
    dict["fixed_current"] = 0.1;
    const Configuration config {std::move(dict)};
    REQUIRE(config.has("output_active"));
    REQUIRE(config.has("OUTPUT_ACTIVE"));
    REQUIRE_FALSE(config.has("output_disabled"));
    REQUIRE(config.count({"fixed_voltage", "fixed_current"}) == 2);
    REQUIRE(config.count({"FIXED_VOLTAGE", "FIXED_CURRENT"}) == 2);
    REQUIRE(config.count({"output_disabled"}) == 0);
    REQUIRE_THROWS_MATCHES(config.count({}), std::invalid_argument, Message("list of keys to count cannot be empty"));
}

TEST_CASE("Configuration scalar getters", "[core][core::config]") {
    Dictionary dict {};
    constexpr bool bool_v = false;
    dict["bool"] = bool_v;
    constexpr int int_v = 16;
    dict["int"] = int_v;
    constexpr double double_v = 1.5;
    dict["double"] = double_v;
    constexpr std::string_view string_v = "hello world";
    dict["string"] = string_v;
    const auto chrono_v = std::chrono::system_clock::now();
    dict["chrono"] = chrono_v;
    constexpr auto enum_v = Enum::A;
    dict["enum"] = enum_v;
    const Configuration config {std::move(dict)};
    // Normal getter
    REQUIRE(config.get<bool>("bool") == bool_v);
    REQUIRE(config.get<int>("int") == int_v);
    REQUIRE(config.get<double>("double") == double_v);
    REQUIRE(config.get<std::string>("string") == string_v);
    REQUIRE(config.get<std::chrono::system_clock::time_point>("chrono") == chrono_v);
    REQUIRE(config.get<Enum>("enum") == enum_v);
    // Default getter
    REQUIRE(config.get<int>("int", int_v + 10) == int_v);
    REQUIRE_FALSE(config.has("int_default"));
    REQUIRE(config.get<int>("int_default", int_v + 15) == int_v + 15);
    REQUIRE(config.has("int_default"));
    // Optional getter
    const auto config_bool_opt = config.getOptional<bool>("bool");
    REQUIRE(config_bool_opt.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE(config_bool_opt.value() == bool_v);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto config_ne_opt = config.getOptional<double>("double_non_existant");
    REQUIRE_FALSE(config_ne_opt.has_value());
}

TEST_CASE("Configuration array getters", "[core][core::config]") {
    Dictionary dict {};
    const std::vector<bool> bool_v {true, true, false};
    dict["bool"] = bool_v;
    const std::vector<int> int_v {1, 2, 3, 4, 5};
    dict["int"] = int_v;
    const std::vector<std::string> string_v {"hello", "world"};
    dict["string"] = string_v;
    dict["single_string"] = "test";
    const Configuration config {std::move(dict)};
    // Normal getter
    REQUIRE_THAT(config.getArray<bool>("bool"), RangeEquals(bool_v));
    REQUIRE_THAT(config.getArray<int>("int"), RangeEquals(int_v));
    REQUIRE_THAT(config.getArray<std::string>("string"), RangeEquals(string_v));
    REQUIRE_THAT(config.getArray<std::string>("single_string"), RangeEquals(std::vector<std::string>({"test"})));
    // Default getter
    REQUIRE_THAT(config.getArray<int>("int", {100, 200, 300}), RangeEquals(int_v));
    REQUIRE_FALSE(config.has("int_default"));
    REQUIRE_THAT(config.getArray<int>("int_default", {10, 20, 30}), RangeEquals(std::vector<int>({10, 20, 30})));
    REQUIRE(config.has("int_default"));
    // Optional getter
    const auto config_bool_opt = config.getOptionalArray<bool>("bool");
    REQUIRE(config_bool_opt.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE_THAT(config_bool_opt.value(), RangeEquals(bool_v));
    const auto config_ne_opt = config.getOptionalArray<double>("double_non_existant");
    REQUIRE_FALSE(config_ne_opt.has_value());
}

TEST_CASE("Configuration set getters", "[core][core::config]") {
    Dictionary dict {};
    const std::vector<std::string> string_v {"A", "A", "B", "C", "B"};
    dict["string"] = string_v;
    dict["single_string"] = "A";
    const Configuration config {std::move(dict)};
    // Normal getter
    REQUIRE_THAT(config.getSet<std::string>("string"), UnorderedRangeEquals(std::vector<std::string>({"A", "B", "C"})));
    REQUIRE_THAT(config.getSet<std::string>("single_string"), UnorderedRangeEquals(std::vector<std::string>({"A"})));
    // Default getter
    const std::set<std::string> default_set_v {"D", "E", "F"};
    REQUIRE_THAT(config.getSet<std::string>("string", default_set_v),
                 UnorderedRangeEquals(std::vector<std::string>({"A", "B", "C"})));
    REQUIRE_FALSE(config.has("string_default"));
    REQUIRE_THAT(config.getSet("string_default", default_set_v), UnorderedRangeEquals(default_set_v));
    REQUIRE(config.has("string_default"));
    // Optional getter
    const auto config_string_opt = config.getOptionalSet<std::string>("string");
    REQUIRE(config_string_opt.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE_THAT(config_string_opt.value(), UnorderedRangeEquals(std::vector<std::string>({"A", "B", "C"})));
    const auto config_ne_opt = config.getOptionalSet<std::string>("double_non_existant");
    REQUIRE_FALSE(config_ne_opt.has_value());
}

TEST_CASE("Configuration path getters", "[core][core::config]") {
    Dictionary dict {};
    const auto absolute_existing_path = test_files_dir() / "good_config.toml";
    dict["absolute_existing_path"] = absolute_existing_path.string();
    const auto absolute_existing_path_2 = test_files_dir() / "good_config.yaml";
    dict["absolute_existing_path_2"] = absolute_existing_path_2.string();
    const auto absolute_nonexistent_path = test_files_dir() / "nonexistent.txt";
    dict["absolute_nonexistent_path"] = absolute_nonexistent_path.string();
    const auto relative_nonexistent_path = std::filesystem::path("nonexistent.txt");
    dict["relative_nonexistent_path"] = relative_nonexistent_path.string();
    dict["absolute_existing_path_array"] = Array({absolute_existing_path.string(), absolute_existing_path_2.string()});
    dict["absolute_nonexistent_path_array"] = Array({absolute_nonexistent_path.string()});
    const Configuration config {std::move(dict)};
    // Normal getter
    const auto absolute_existing_path_r = config.getPath("absolute_existing_path", true);
    REQUIRE(absolute_existing_path_r == absolute_existing_path);
    REQUIRE_THAT(absolute_existing_path_r.string(), EndsWith("good_config.toml"));
    const auto absolute_existing_path_2_r = config.getPath("absolute_existing_path_2", false);
    REQUIRE(absolute_existing_path_2_r == absolute_existing_path_2);
    REQUIRE_THAT(absolute_existing_path_2_r.string(), EndsWith("good_config.yaml"));
    const auto absolute_nonexistent_path_r = config.getPath("absolute_nonexistent_path", false);
    REQUIRE(absolute_nonexistent_path_r == absolute_nonexistent_path);
    REQUIRE_THROWS_MATCHES(config.getPath("absolute_nonexistent_path", true),
                           InvalidValueError,
                           Message("Value of key `absolute_nonexistent_path` is not valid: path `" +
                                   absolute_nonexistent_path.string() + "` not found"));
    const auto relative_nonexistent_path_r = config.getPath("relative_nonexistent_path", false);
    REQUIRE(relative_nonexistent_path_r.is_absolute());
    // Array getter
    REQUIRE_THAT(config.getPathArray("absolute_existing_path_array", true),
                 RangeEquals(std::vector({absolute_existing_path, absolute_existing_path_2})));
    REQUIRE_THAT(config.getPathArray("absolute_nonexistent_path_array", false),
                 RangeEquals(std::vector({absolute_nonexistent_path})));
    REQUIRE_THROWS_MATCHES(config.getPathArray("absolute_nonexistent_path_array", true),
                           InvalidValueError,
                           Message("Value of key `absolute_nonexistent_path_array` is not valid: path `" +
                                   absolute_nonexistent_path.string() + "` not found"));
}

TEST_CASE("Configuration section getters", "[core][core::config]") {
    Dictionary dict {};
    dict["int"] = 5;
    Dictionary subdict_1 {};
    subdict_1["int"] = 4;
    dict["sub_1"] = std::move(subdict_1);
    Dictionary subdict_2 {};
    subdict_2["int"] = 3;
    Dictionary subsubdict {};
    subsubdict["int"] = 2;
    Dictionary subsubsubdict {};
    subsubsubdict["int"] = 1;
    subsubdict["sub"] = std::move(subsubsubdict);
    subdict_2["sub"] = std::move(subsubdict);
    dict["sub_2"] = std::move(subdict_2);
    // Check configuration matches
    const Configuration config {std::move(dict)};
    REQUIRE(config.get<int>("int") == 5);
    const auto& config_subdict_1 = config.getSection("sub_1");
    REQUIRE(config_subdict_1.get<int>("int") == 4);
    const auto& config_subdict_2 = config.getSection("sub_2");
    REQUIRE(config_subdict_2.get<int>("int") == 3);
    const auto& config_subsubdict = config_subdict_2.getSection("sub");
    REQUIRE(config_subsubdict.get<int>("int") == 2);
    const auto& config_subsubsubdict = config_subsubdict.getSection("sub");
    REQUIRE(config_subsubsubdict.get<int>("int") == 1);
    // Check missing key
    REQUIRE_THROWS_MATCHES(
        config_subsubsubdict.getSection("sub"), MissingKeyError, Message("Key `sub_2.sub.sub.sub` does not exist"));
    // Check not section
    REQUIRE_THROWS_MATCHES(config_subsubsubdict.getSection("int"),
                           InvalidTypeError,
                           Message("Could not convert value of type `" + demangle<std::int64_t>() +
                                   "` to type `Section` for key `sub_2.sub.sub.int`"));
    // Optional getter
    const auto config_subdict_1_opt = config.getOptionalSection("sub_1");
    REQUIRE(config_subdict_1_opt.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE(config_subdict_1_opt.value().get().get<int>("int") == 4);
    const auto config_ne_opt = config.getOptionalSection("non_existant");
    REQUIRE_FALSE(config_ne_opt.has_value());
}

TEST_CASE("Configuration section keys", "[core][core::config]") {
    Dictionary dict {};
    dict["bool"] = true;
    dict["int"] = 5;
    dict["subdict_1"] = Dictionary({{"hello", 1}, {"world", 2}});
    dict["subdict_2"] = Dictionary({{"1", 1}, {"2", 4}, {"3", 9}, {"4", 16}});
    const Configuration config {std::move(dict)};
    REQUIRE_THAT(config.getKeys(), RangeEquals(std::vector<std::string>({"bool", "int", "subdict_1", "subdict_2"})));
}

TEST_CASE("Configuration get text", "[core][core::config]") {
    Dictionary dict {};
    dict["bool"] = true;
    dict["int"] = 5;
    const Configuration config {std::move(dict)};
    REQUIRE_THAT(config.getText("bool"), Equals("true"));
    REQUIRE_THAT(config.getText("int"), Equals("5"));
    REQUIRE_THROWS_MATCHES(config.getText("ne"), MissingKeyError, Message("Key `ne` does not exist"));
}

TEST_CASE("Configuration missing key", "[core][core::config]") {
    Dictionary dict {};
    Dictionary subdict {};
    subdict["key"] = true;
    dict["sub"] = std::move(subdict);
    const Configuration config {std::move(dict)};
    const auto& sub_config = config.getSection("Sub");
    REQUIRE_THROWS_MATCHES(sub_config.get<int>("Key2"), MissingKeyError, Message("Key `sub.Key2` does not exist"));
}

TEST_CASE("Configuration invalid values", "[core][core::config]") {
    Dictionary dict {};
    dict["int"] = -1;
    Dictionary enum_dict {};
    enum_dict["c"] = "C";
    dict["enum"] = std::move(enum_dict);
    const Configuration config {std::move(dict)};
    REQUIRE_THROWS_MATCHES(
        config.get<std::uint32_t>("int"),
        InvalidValueError,
        Message("Value of key `int` is not valid: value `-1` is out of range for `" + demangle<std::uint32_t>() + "`"));
    const auto& config_enum = config.getSection("enum");
    REQUIRE_THROWS_MATCHES(config_enum.get<Enum>("c"),
                           InvalidValueError,
                           Message("Value of key `enum.c` is not valid: value `C` is not valid, possible values are A, B"));
}

TEST_CASE("Configuration aliases", "[core][core::config]") {
    // Alias used
    Dictionary dict_old {};
    dict_old["old"] = 1;
    const Configuration config_old {std::move(dict_old)};
    REQUIRE(config_old.has("old"));
    config_old.setAlias("new", "old");
    REQUIRE(config_old.get<int>("new") == 1);
    REQUIRE_FALSE(config_old.has("old"));
    // Alias not used
    Dictionary dict_new {};
    dict_new["new"] = 1;
    const Configuration config_new {std::move(dict_new)};
    REQUIRE(config_new.has("new"));
    config_new.setAlias("new", "old");
    REQUIRE(config_new.has("new"));
    REQUIRE_FALSE(config_new.has("old"));
    REQUIRE(config_new.get<int>("new") == 1);
    // Alias not in configuration
    Dictionary dict {};
    dict["something_else"] = 1;
    const Configuration config {std::move(dict)};
    config.setAlias("new", "old");
    REQUIRE_FALSE(config.has("new"));
    REQUIRE_FALSE(config.has("old"));
}

TEST_CASE("Configuration case-insensitivity", "[core][core::config]") {
    Dictionary dict {};
    constexpr auto bool_v = true;
    dict["BOOL"] = bool_v;
    constexpr auto int_v = 5;
    dict["inT"] = int_v;
    constexpr std::string_view string_v = "hello world";
    dict["sTrInG"] = string_v;
    const Configuration config {std::move(dict)};
    REQUIRE(config.get<bool>("bOoL") == bool_v);
    REQUIRE(config.get<int>("INT") == int_v);
    REQUIRE(config.get<std::string>("StRiNg") == string_v);
}

TEST_CASE("Configuration case-insensitivity during construction", "[core][core::config]") {
    Dictionary dict {};
    dict["BOOL"] = true;
    dict["bool"] = true;
    REQUIRE_THROWS_MATCHES(
        Configuration(std::move(dict)), InvalidKeyError, Message("Key `bool` is not valid: key defined twice"));
}

TEST_CASE("Configuration string conversion", "[core][core::config]") {
    Dictionary dict {};
    dict["_internal"] = 1024;
    dict["user"] = 3.14;
    Dictionary subdict_1 {};
    subdict_1["array"] = Array({1, 2, 3, 4});
    dict["sub_1"] = std::move(subdict_1);
    Dictionary subdict_2 {};
    subdict_2["enum"] = Enum::A;
    Dictionary subsubdict {};
    subsubdict["string"] = "hello world";
    subdict_2["sub"] = std::move(subsubdict);
    dict["sub_2"] = std::move(subdict_2);
    const Configuration config {std::move(dict)};
    REQUIRE_THAT(config.to_string(Configuration::ALL),
                 Equals("\n"
                        "  _internal: 1024\n"
                        "  sub_1:\n"
                        "    array: [ 1, 2, 3, 4 ]\n"
                        "  sub_2:\n"
                        "    enum: A\n"
                        "    sub:\n"
                        "      string: hello world\n"
                        "  user: 3.14"));
    REQUIRE_THAT(config.to_string(Configuration::USER),
                 Equals("\n"
                        "  sub_1:\n"
                        "    array: [ 1, 2, 3, 4 ]\n"
                        "  sub_2:\n"
                        "    enum: A\n"
                        "    sub:\n"
                        "      string: hello world\n"
                        "  user: 3.14"));
    REQUIRE_THAT(config.to_string(Configuration::INTERNAL),
                 Equals("\n"
                        "  _internal: 1024"));
}

TEST_CASE("Configuration unused keys", "[core][core::config]") {
    Dictionary dict {};
    dict["used"] = 1024;
    dict["unused"] = 1024;
    Dictionary subdict {};
    subdict["used"] = 2048;
    subdict["unused"] = 2048;
    Dictionary subsubdict {};
    subsubdict["unused"] = 4096;
    subdict["sub"] = std::move(subsubdict);
    dict["sub"] = std::move(subdict);
    Configuration config {std::move(dict)};
    // Mark keys as used
    REQUIRE(config.get<int>("used") == 1024);
    const auto& sub_config = config.getSection("sub");
    REQUIRE(sub_config.get<int>("used") == 2048);
    // Move configuration
    Configuration config_moved {std::move(config)};
    Configuration config_assigned {};
    config_assigned = std::move(config_moved);
    // Check existence of unused keys
    REQUIRE(config_assigned.has("unused"));
    // Remove unused keys
    const auto removed_keys = config_assigned.removeUnusedEntries();
    REQUIRE_THAT(removed_keys, UnorderedRangeEquals(std::vector<std::string>({"unused", "sub.unused", "sub.sub.unused"})));
    // Check unused keys were removed
    REQUIRE_FALSE(config_assigned.has("unused"));
    const auto& sub_config_after = config_assigned.getSection("sub");
    REQUIRE(sub_config_after.has("used"));
    REQUIRE_FALSE(sub_config_after.has("sub"));
}

TEST_CASE("Configuration update", "[core][core::config]") {
    Dictionary dict {};
    dict["bool"] = true;
    dict["int"] = 1024;
    dict["array_empty"] = Array();
    dict["array_int"] = Array({1, 2, 3});
    dict["array_int2"] = Array({1, 2, 3, 4, 5});
    Dictionary subdict {};
    subdict["double"] = 3.14;
    subdict["string"] = "test";
    dict["sub"] = std::move(subdict);
    Configuration config {std::move(dict)};
    Dictionary dict_update {};
    dict_update["bool"] = false;
    dict_update["int"] = 2048;
    dict_update["array_empty"] = Array({1, 2});
    dict_update["array_int"] = Array();
    dict_update["array_int2"] = Array({1, 2, 3, 4});
    Dictionary subdict_update {};
    subdict_update["double"] = 6.28;
    dict_update["sub"] = std::move(subdict_update);
    config.update(Configuration(std::move(dict_update)));
    REQUIRE(config.get<bool>("bool") == false);
    REQUIRE(config.get<int>("int") == 2048);
    REQUIRE_THAT(config.getArray<int>("array_empty"), RangeEquals(std::vector<int>({1, 2})));
    REQUIRE_THAT(config.getArray<int>("array_int"), RangeEquals(std::vector<int>({})));
    REQUIRE_THAT(config.getArray<int>("array_int2"), RangeEquals(std::vector<int>({1, 2, 3, 4})));
    const auto& config_sub = config.getSection("sub");
    REQUIRE(config_sub.get<double>("double") == 6.28);
    REQUIRE_THAT(config_sub.get<std::string>("string"), Equals("test"));
}

TEST_CASE("Configuration update failure", "[core][core::config]") {
    Dictionary dict {};
    dict["bool"] = true;
    dict["int"] = 1024;
    dict["array"] = Array({1.5, 2.5, 3.5});
    Configuration config {std::move(dict)};
    // Updating non-existing key
    Dictionary dict_update_ne_key {};
    dict_update_ne_key["bool2"] = false;
    REQUIRE_THROWS_MATCHES(config.update(Configuration(std::move(dict_update_ne_key))),
                           InvalidUpdateError,
                           Message("Failed to update value of key `bool2`: key does not exist in current configuration"));
    // Updating with other type
    Dictionary dict_update_type {};
    dict_update_type["bool"] = Array();
    REQUIRE_THROWS_MATCHES(config.update(Configuration(std::move(dict_update_type))),
                           InvalidUpdateError,
                           Message("Failed to update value of key `bool`: cannot change type from `bool` to `Array`"));
    // Updating with other scalar type
    Dictionary dict_update_scalar_type {};
    dict_update_scalar_type["bool"] = "true";
    REQUIRE_THROWS_MATCHES(config.update(Configuration(std::move(dict_update_scalar_type))),
                           InvalidUpdateError,
                           Message("Failed to update value of key `bool`: cannot change type from `bool` to `std::string`"));
    // Updating with other array type
    Dictionary dict_update_array_type {};
    dict_update_array_type["array"] = Array({"hello", "world"});
    REQUIRE_THROWS_MATCHES(
        config.update(Configuration(std::move(dict_update_array_type))),
        InvalidUpdateError,
        Message("Failed to update value of key `array`: cannot change type from `Array<double>` to `Array<std::string>`"));
}

TEST_CASE("Configuration message assembly & disassembly", "[core][core::config]") {
    Dictionary dict {};
    dict["bool"] = true;
    dict["int"] = 5;
    dict["subdict"] = Dictionary({{"hello", 1}, {"world", 2}});
    const Configuration config {std::move(dict)};
    const auto message = config.assemble();
    REQUIRE(config.asDictionary() == Configuration::disassemble(message).asDictionary());
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
