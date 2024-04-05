/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <ctime>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Set & Get Values", "[core][core::config]") {
    Configuration config;

    config.set("bool", true);

    config.set("int64", std::int64_t(63));
    config.set("size", size_t(1));
    config.set("uint64", std::uint64_t(64));
    config.set("uint8", std::uint8_t(8));

    config.set("double", double(1.3));
    config.set("float", float(3.14));

    config.set("string", std::string("a"));

    enum MyEnum {
        ONE,
        TWO,
    };
    config.set("myenum", MyEnum::ONE);

    auto tp = std::chrono::system_clock::now();
    config.set("time", tp);

    // Check that keys are unused
    REQUIRE(config.size() == config.getUnusedKeys().size());

    // Read values back
    REQUIRE(config.get<bool>("bool") == true);

    REQUIRE(config.get<std::int64_t>("int64") == 63);
    REQUIRE(config.get<size_t>("size") == 1);
    REQUIRE(config.get<std::uint64_t>("uint64") == 64);
    REQUIRE(config.get<std::uint8_t>("uint8") == 8);

    REQUIRE(config.get<double>("double") == 1.3);
    REQUIRE(config.get<float>("float") == 3.14F);

    REQUIRE(config.get<std::string>("string") == "a");

    REQUIRE(config.get<MyEnum>("myenum") == MyEnum::ONE);

    REQUIRE(config.get<std::chrono::system_clock::time_point>("time") == tp);

    // Check that all keys have been marked as used
    REQUIRE(config.getUnusedKeys().empty());
}

TEST_CASE("Set & Get Array Values", "[core][core::config]") {
    Configuration config;

    config.setArray<double>("myarray", {12., 14., 16.});
    REQUIRE(config.getArray<double>("myarray") == std::vector<double>({12., 14., 16.}));

    // config.set<std::vector<size_t>>("my_size_t_vector", {1, 2, 3});
    config.setArray<size_t>("my_size_t_array", {1, 2, 3});
    REQUIRE(config.getArray<size_t>("my_size_t_array") == std::vector<size_t>({1, 2, 3}));
}

TEST_CASE("Set Value & Mark Used", "[core][core::config]") {
    Configuration config;

    config.set("myval", 3.14, true);

    // Check that the key is marked as used
    REQUIRE(config.getUnusedKeys().empty());
    REQUIRE(config.get<double>("myval") == 3.14);
}

TEST_CASE("Set Default Value", "[core][core::config]") {
    Configuration config;

    // Check that a default does not overwrite existing values
    config.set("myval", true);
    config.setDefault("myval", false);
    REQUIRE(config.get<bool>("myval") == true);

    // Check that a default is set when the value does not exist
    config.setDefault("mydefault", false);
    REQUIRE(config.get<bool>("mydefault") == false);
}

TEST_CASE("Set & Use Aliases", "[core][core::config]") {
    Configuration config;

    // Alias set before key exists
    config.setAlias("thisisnotset", "mykey");

    // Set key
    config.set("mykey", 99);

    // Set alias to key
    config.setAlias("thisisset", "mykey");

    // Check that the alias set before the key existed is not set:
    REQUIRE(config.has("thisisnotset") == false);

    // Check that the new key is accessible
    REQUIRE(config.get<size_t>("thisisset") == 99);

    // Set second key
    config.set("myotherkey", 77);
    // Attempt to set an alias for second key
    config.setAlias("mykey", "myotherkey");

    // Check that the alias would not overwrite another existing key:
    REQUIRE(config.get<size_t>("mykey") == 99);
}

TEST_CASE("Invalid Key Access", "[core][core::config]") {
    Configuration config;

    // Check for invalid key to be detected
    REQUIRE_THROWS_AS(config.get<bool>("invalidkey"), MissingKeyError);
    REQUIRE_THROWS_MATCHES(config.get<bool>("invalidkey"), MissingKeyError, Message("Key 'invalidkey' does not exist"));

    // Check for invalid key to be detected when querying text representation
    REQUIRE_THROWS_AS(config.getText("invalidkey"), MissingKeyError);
    REQUIRE_THROWS_MATCHES(config.getText("invalidkey"), MissingKeyError, Message("Key 'invalidkey' does not exist"));

    // Check for invalid type conversion
    config.set("key", true);
    REQUIRE_THROWS_AS(config.get<double>("key"), InvalidTypeError);
    REQUIRE_THROWS_MATCHES(config.get<double>("key"),
                           InvalidTypeError,
                           Message("Could not convert value of type 'bool' to type 'double' for key 'key'"));

    // Check for invalid enum value conversion:
    enum MyEnum {
        ONE,
        TWO,
    };
    config.set("myenum", "THREE");
    REQUIRE_THROWS_AS(config.get<MyEnum>("myenum"), InvalidValueError);
    REQUIRE_THROWS_MATCHES(config.get<MyEnum>("myenum"),
                           InvalidValueError,
                           Message("Value THREE of key 'myenum' is not valid: possible values are one, two"));
}

TEST_CASE("Merge Configurations", "[core][core::config]") {
    Configuration config_a;
    Configuration config_b;

    config_a.set("bool", true);
    config_a.set("int64", std::int64_t(63));

    config_b.set("bool", false);
    config_b.set("uint64", std::uint64_t(64));

    // Merge configurations
    config_a.merge(config_b);

    // Check that keys from config_b have been transferred:
    REQUIRE(config_a.get<std::uint64_t>("uint64") == 64);

    // Check that existing keys in config_a have not been overwritten
    REQUIRE(config_a.get<bool>("bool") == true);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
