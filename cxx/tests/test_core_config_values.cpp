/**
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/value_types.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::utils;
using namespace std::string_literals;
using namespace std::string_view_literals;

enum class Enum : std::uint8_t { A, B };

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

// --- Scalar ---

TEST_CASE("Scalar direct constructors and getters", "[core][core::config]") {
    // bool
    constexpr auto bool_v = true;
    const Scalar scalar_bool {bool_v};
    REQUIRE(scalar_bool.get<bool>() == bool_v);
    // int64
    constexpr auto int64_v = static_cast<std::int64_t>(-512);
    const Scalar scalar_int64 {int64_v};
    REQUIRE(scalar_int64.get<std::int64_t>() == int64_v);
    // double
    constexpr auto double_v = 3.14;
    const Scalar scalar_double {double_v};
    REQUIRE(scalar_double.get<double>() == double_v);
    // string
    const auto string_v = "string"s;
    const Scalar scalar_string {string_v};
    REQUIRE(scalar_string.get<std::string>() == string_v);
    // chrono time point
    const auto chrono_v = std::chrono::system_clock::now();
    const Scalar scalar_chrono {chrono_v};
    REQUIRE(scalar_chrono.get<std::chrono::system_clock::time_point>() == chrono_v);
}

TEST_CASE("Scalar indirect constructors and getters", "[core][core::config]") {
    // int
    constexpr auto uint32_v = static_cast<std::uint32_t>(2048);
    const Scalar scalar_uint32 {uint32_v};
    REQUIRE(scalar_uint32.get<std::uint32_t>() == uint32_v);
    // float
    constexpr auto float_v = 1.2345F;
    const Scalar scalar_float {float_v};
    REQUIRE(scalar_float.get<float>() == float_v);
    // string
    const char* cstring_v = "cstring";
    const Scalar scalar_cstring {cstring_v};
    REQUIRE(scalar_cstring.get<std::string>() == cstring_v);
    constexpr auto string_view_v = "string_view"sv;
    const Scalar scalar_string_view {string_view_v};
    REQUIRE(scalar_string_view.get<std::string_view>() == string_view_v);
    // enum
    constexpr auto enum_v = Enum::A;
    const Scalar scalar_enum {enum_v};
    REQUIRE(scalar_enum.get<Enum>() == enum_v);
}

TEST_CASE("Scalar default constructor", "[core][core::config]") {
    const auto scalar_valueless = Scalar();
    // Casting always fails with bad variant access
    REQUIRE_THROWS_AS(scalar_valueless.get<bool>(), std::bad_variant_access);
}

TEST_CASE("Scalar invalid integer argument", "[core][core::config]") {
    // Test constructor throws
    constexpr auto uint64_max = std::numeric_limits<std::uint64_t>::max();
    REQUIRE_THROWS_MATCHES(
        Scalar(uint64_max),
        std::invalid_argument,
        Message("value " + quote(to_string(uint64_max)) + " is out of range for " + quote(demangle<std::int64_t>())));
    // Test getter throws
    constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();
    const Scalar scalar = {int64_max};
    REQUIRE_THROWS_MATCHES(
        scalar.get<std::int32_t>(),
        std::invalid_argument,
        Message("value " + quote(to_string(int64_max)) + " is out of range for " + quote(demangle<std::int32_t>())));
}

TEST_CASE("Scalar invalid enum argument", "[core][core::config]") {
    const Scalar scalar {"C"};
    REQUIRE_THROWS_MATCHES(scalar.get<Enum>(),
                           std::invalid_argument,
                           Message("value " + quote("C") + " is not valid, possible values are " + list_enum_names<Enum>()));
}

TEST_CASE("Scalar operators", "[core][core::config]") {
    Scalar scalar {};
    REQUIRE_FALSE(scalar == false);
    scalar = true;
    REQUIRE(scalar == true);
    REQUIRE_FALSE(scalar == 0);
    scalar = 3.0F;
    REQUIRE(scalar == 3.0);
    REQUIRE(scalar == 3);
    REQUIRE(scalar > -2);
    REQUIRE(scalar < 255);
    scalar = 4;
    REQUIRE(scalar == 4);
    REQUIRE(scalar == 4.0);
    REQUIRE(scalar > -5);
    REQUIRE(scalar < 512);
    scalar = "string";
    REQUIRE(scalar == "string");
    REQUIRE_FALSE(scalar == 4);
}

TEST_CASE("Scalar string conversion", "[core][core::config]") {
    Scalar scalar {};
    REQUIRE_THAT(scalar.to_string(), Equals("NIL"));
    scalar = true;
    REQUIRE_THAT(scalar.to_string(), Equals("true"));
    scalar = -5123;
    REQUIRE_THAT(scalar.to_string(), Equals("-5123"));
    scalar = 2.5;
    REQUIRE_THAT(scalar.to_string(), Equals("2.5"));
    scalar = 1.0;
    REQUIRE_THAT(scalar.to_string(), Equals("1.0"));
    scalar = "test";
    REQUIRE_THAT(scalar.to_string(), Equals("test"));
    scalar = std::chrono::system_clock::time_point();
    REQUIRE_THAT(scalar.to_string(), RegexMatcher("1970-01-01 00:00:00.0{6,}", {}));
}

TEST_CASE("Scalar type demangling", "[core][core::config]") {
    Scalar scalar {};
    REQUIRE_THAT(scalar.demangle(), Equals("NIL"));
    scalar = true;
    REQUIRE_THAT(scalar.demangle(), Equals("bool"));
    scalar = 0;
    REQUIRE_THAT(scalar.demangle(), Equals(demangle<std::int64_t>()));
    scalar = 1.0;
    REQUIRE_THAT(scalar.demangle(), Equals("double"));
    scalar = "test";
    REQUIRE_THAT(scalar.demangle(), Equals("std::string"));
    scalar = std::chrono::system_clock::time_point();
    REQUIRE_THAT(scalar.demangle(), Equals("std::chrono::system_clock::time_point"));
}

TEST_CASE("Scalar msgpack packing & unpacking", "[core][core::config]") {
    // NIL
    Scalar scalar {};
    msgpack::sbuffer sbuffer_valueless {};
    msgpack_pack(sbuffer_valueless, scalar);
    const auto scalar_valueless = msgpack_unpack_to<Scalar>(sbuffer_valueless.data(), sbuffer_valueless.size());
    REQUIRE(scalar == scalar_valueless);
    // BOOLEAN
    scalar = true;
    msgpack::sbuffer sbuffer_bool {};
    msgpack_pack(sbuffer_bool, scalar);
    const auto scalar_bool = msgpack_unpack_to<Scalar>(sbuffer_bool.data(), sbuffer_bool.size());
    REQUIRE(scalar == scalar_bool);
    // INTEGER
    scalar = -123456;
    msgpack::sbuffer sbuffer_int {};
    msgpack_pack(sbuffer_int, scalar);
    const auto scalar_int = msgpack_unpack_to<Scalar>(sbuffer_int.data(), sbuffer_int.size());
    REQUIRE(scalar == scalar_int);
    // FLOAT
    scalar = 1.3579F;
    msgpack::sbuffer sbuffer_double {};
    msgpack_pack(sbuffer_double, scalar);
    const auto scalar_double = msgpack_unpack_to<Scalar>(sbuffer_double.data(), sbuffer_double.size());
    REQUIRE(scalar == scalar_double);
    // STR
    scalar = "string";
    msgpack::sbuffer sbuffer_string {};
    msgpack_pack(sbuffer_string, scalar);
    const auto scalar_string = msgpack_unpack_to<Scalar>(sbuffer_string.data(), sbuffer_string.size());
    REQUIRE(scalar == scalar_string);
    // EXT
    scalar = std::chrono::system_clock::now();
    msgpack::sbuffer sbuffer_chrono {};
    msgpack_pack(sbuffer_chrono, scalar);
    const auto scalar_chrono = msgpack_unpack_to<Scalar>(sbuffer_chrono.data(), sbuffer_chrono.size());
    REQUIRE(scalar == scalar_chrono);
    // Unsupported type
    msgpack::sbuffer sbuffer_unsupported {};
    msgpack_pack(sbuffer_unsupported, std::vector<int>({1, 2, 3}));
    REQUIRE_THROWS_AS(msgpack_unpack_to<Scalar>(sbuffer_unsupported.data(), sbuffer_unsupported.size()), MsgpackUnpackError);
}

// --- Array ---

TEST_CASE("Array direct constructors and getters", "[core][core::config]") {
    // bool / vector
    const std::vector<bool> bool_v {true, false, true};
    const Array array_bool {bool_v};
    REQUIRE_THAT(array_bool.getVector<bool>(), RangeEquals(bool_v));
    REQUIRE_FALSE(array_bool.empty());
    // int64 / list
    const std::list<std::int64_t> int64_v {1, 2, 3, 4, 5};
    const Array array_int64 {int64_v};
    REQUIRE_THAT(array_int64.getVector<std::int64_t>(), RangeEquals(int64_v));
    REQUIRE_FALSE(array_int64.empty());
    // double / deque
    const std::deque<double> double_v {1.1};
    const Array array_double {double_v};
    REQUIRE_THAT(array_double.getVector<double>(), RangeEquals(double_v));
    REQUIRE_FALSE(array_double.empty());
    // string / set
    const std::set<std::string> string_v {"hello", "world"};
    const Array array_string {string_v};
    REQUIRE_THAT(array_string.getVector<std::string>(), RangeEquals(string_v));
    REQUIRE_FALSE(array_string.empty());
    // chrono time stamp / array
    const std::array<std::chrono::system_clock::time_point, 1> chrono_v {std::chrono::system_clock::now()};
    const Array array_chrono {chrono_v};
    REQUIRE_THAT(array_chrono.getVector<std::chrono::system_clock::time_point>(), RangeEquals(chrono_v));
    REQUIRE_FALSE(array_chrono.empty());
}

TEST_CASE("Array indirect constructors and getters", "[core][core::config]") {
    // int
    const std::vector<std::uint32_t> uint32_v {2048, 4096, 8192};
    const Array array_uint32 {uint32_v};
    REQUIRE_THAT(array_uint32.getVector<std::uint32_t>(), RangeEquals(uint32_v));
    // float
    const std::vector<float> float_v {1.0F, 2.0F, 3.0F, 4.0F, 5.0F};
    const Array array_float {float_v};
    REQUIRE_THAT(array_float.getVector<float>(), RangeEquals(float_v));
    // string
    const std::vector<const char*> cstring_v = {"cstring1", "cstring2"};
    const Array array_cstring {cstring_v};
    REQUIRE_THAT(array_cstring.getVector<std::string>(), RangeEquals(cstring_v));
    const std::vector<std::string_view> string_view_v {"string_view"sv};
    const Array array_string_view {string_view_v};
    REQUIRE_THAT(array_string_view.getVector<std::string_view>(), RangeEquals(string_view_v));
    // enum
    const std::vector<Enum> enum_v = {Enum::A, Enum::B};
    const Array array_enum {enum_v};
    REQUIRE_THAT(array_enum.getVector<Enum>(), RangeEquals(enum_v));
}

TEST_CASE("Array default and empty constructor", "[core][core::config]") {
    // Default constructor
    const auto array_default = Array();
    REQUIRE(array_default.empty());
    // Empty constructor
    std::vector<int> empty_v {};
    const Array array_empty {empty_v};
    REQUIRE(array_empty.empty());
    REQUIRE_THAT(array_empty.getVector<int>(), RangeEquals(empty_v));
}

TEST_CASE("Array invalid integer argument", "[core][core::config]") {
    // Test constructor throws
    constexpr auto uint64_max = std::numeric_limits<std::uint64_t>::max();
    const std::vector<std::uint64_t> uint64_v = {1, 2, 3, uint64_max, 5};
    REQUIRE_THROWS_MATCHES(
        Array(uint64_v),
        std::invalid_argument,
        Message("value " + quote(to_string(uint64_max)) + " is out of range for " + quote(demangle<std::int64_t>())));
    // Test getter throws
    constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();
    const std::vector<std::int64_t> int64_v = {1, 2, 3, int64_max, 5};
    const Array array = {int64_v};
    REQUIRE_THROWS_MATCHES(
        array.getVector<std::int32_t>(),
        std::invalid_argument,
        Message("value " + quote(to_string(int64_max)) + " is out of range for " + quote(demangle<std::int32_t>())));
}

TEST_CASE("Array invalid enum argument", "[core][core::config]") {
    const Array array {"A", "B", "C", "D"};
    REQUIRE_THROWS_MATCHES(array.getVector<Enum>(),
                           std::invalid_argument,
                           Message("value " + quote("C") + " is not valid, possible values are " + list_enum_names<Enum>()));
}

TEST_CASE("Array operators", "[core][core::config]") {
    Array array {};
    REQUIRE(array == std::vector<int>());
    REQUIRE_FALSE(array == std::vector<bool>({false}));
    array = {1, 0};
    REQUIRE(array == std::vector<int>({1, 0}));
    REQUIRE_FALSE(array == std::vector<bool>({true, false}));
    array = {3.0F, 4.0F};
    REQUIRE(array == std::vector<float>({3.0F, 4.0F}));
    REQUIRE_FALSE(array == std::vector<int>({3, 4}));
    array = {"hello"};
    REQUIRE(array == std::vector<std::string>({"hello"}));
    REQUIRE_FALSE(array == std::vector<std::string>({"hello", "world"}));
}

TEST_CASE("Array string conversion", "[core][core::config]") {
    Array array {};
    REQUIRE_THAT(array.to_string(), Equals("[]"));
    array = {true, false};
    REQUIRE_THAT(array.to_string(), Equals("[ true, false ]"));
    array = {-5123, 4};
    REQUIRE_THAT(array.to_string(), Equals("[ -5123, 4 ]"));
    array = {2.5, 3.5, 4.5, 5.5};
    REQUIRE_THAT(array.to_string(), Equals("[ 2.5, 3.5, 4.5, 5.5 ]"));
    array = {"hello", "world"};
    REQUIRE_THAT(array.to_string(), Equals("[ hello, world ]"));
    array = {std::chrono::system_clock::time_point()};
    REQUIRE_THAT(array.to_string(), RegexMatcher("\\[ 1970-01-01 00:00:00.0{6,} \\]", {}));
}

TEST_CASE("Array type demangling", "[core][core::config]") {
    Array array {};
    REQUIRE_THAT(array.demangle(), Equals("Array"));
    array = {true};
    REQUIRE_THAT(array.demangle(), Equals("Array<bool>"));
    array = {0};
    REQUIRE_THAT(array.demangle(), Equals("Array<" + demangle<std::int64_t>() + ">"));
    array = {1.0};
    REQUIRE_THAT(array.demangle(), Equals("Array<double>"));
    array = {"test"};
    REQUIRE_THAT(array.demangle(), Equals("Array<std::string>"));
    array = {std::chrono::system_clock::time_point()};
    REQUIRE_THAT(array.demangle(), Equals("Array<std::chrono::system_clock::time_point>"));
}

TEST_CASE("Array msgpack packing & unpacking", "[core][core::config]") {
    // Empty
    Array array {};
    msgpack::sbuffer sbuffer_empty {};
    msgpack_pack(sbuffer_empty, array);
    const auto array_empty = msgpack_unpack_to<Array>(sbuffer_empty.data(), sbuffer_empty.size());
    REQUIRE(array == array_empty);
    // BOOLEAN
    array = {true, true, false};
    msgpack::sbuffer sbuffer_bool {};
    msgpack_pack(sbuffer_bool, array);
    const auto array_bool = msgpack_unpack_to<Array>(sbuffer_bool.data(), sbuffer_bool.size());
    REQUIRE(array == array_bool);
    // INTEGER
    array = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    msgpack::sbuffer sbuffer_int {};
    msgpack_pack(sbuffer_int, array);
    const auto array_int = msgpack_unpack_to<Array>(sbuffer_int.data(), sbuffer_int.size());
    REQUIRE(array == array_int);
    // FLOAT
    array = {1.0, 2.0, 3.0, 4.0};
    msgpack::sbuffer sbuffer_double {};
    msgpack_pack(sbuffer_double, array);
    const auto array_double = msgpack_unpack_to<Array>(sbuffer_double.data(), sbuffer_double.size());
    REQUIRE(array == array_double);
    // STR
    array = {"hello", "world"};
    msgpack::sbuffer sbuffer_string {};
    msgpack_pack(sbuffer_string, array);
    const auto array_string = msgpack_unpack_to<Array>(sbuffer_string.data(), sbuffer_string.size());
    REQUIRE(array == array_string);
    // EXT
    array = {std::chrono::system_clock::now()};
    msgpack::sbuffer sbuffer_chrono {};
    msgpack_pack(sbuffer_chrono, array);
    const auto array_chrono = msgpack_unpack_to<Array>(sbuffer_chrono.data(), sbuffer_chrono.size());
    REQUIRE(array == array_chrono);
    // Not an array
    msgpack::sbuffer sbuffer_notarray {};
    msgpack_pack(sbuffer_notarray, "test");
    REQUIRE_THROWS_AS(msgpack_unpack_to<Array>(sbuffer_notarray.data(), sbuffer_notarray.size()), MsgpackUnpackError);
    // Unsupported type
    msgpack::sbuffer sbuffer_unsupported {};
    msgpack_pack(sbuffer_unsupported, std::vector<std::vector<int>>({{1, 2}, {3, 4}, {5, 6}}));
    REQUIRE_THROWS_AS(msgpack_unpack_to<Array>(sbuffer_unsupported.data(), sbuffer_unsupported.size()), MsgpackUnpackError);
}

// --- Dictionary ---

TEST_CASE("Dictionary map constructor and getter", "[core][core::config]") {
    const std::map<std::string, int> map_v {{"hello", 1}, {"world", 2}};
    const Dictionary dict {map_v};
    REQUIRE_THAT(dict.getMap<int>(), RangeEquals(map_v));
}

TEST_CASE("Dictionary default constructor", "[core][core::config]") {
    const auto dict = Dictionary();
    REQUIRE(dict.empty());
    REQUIRE(dict.getMap<int>().empty());
}

TEST_CASE("Dictionary operators", "[core][core::config]") {
    Dictionary dict {};
    REQUIRE(dict == std::map<std::string, int>()); // NOLINT(readability-container-size-empty)
    REQUIRE_FALSE(dict == std::map<std::string, std::string>({{"hello", "world"}}));
    const std::map<std::string, int> map_v {{"hello", 1}, {"world", 2}};
    dict = map_v;
    REQUIRE(dict == map_v);
    REQUIRE_FALSE(dict == std::map<std::string, int>({{"hello", 1}}));
}

TEST_CASE("Dictionary flattened", "[core][core::config]") {
    Dictionary dict {};
    dict["sub_1"] = Dictionary();
    Dictionary subdict {};
    subdict["int"] = 1024;
    Dictionary subsubdict {};
    subsubdict["int"] = 2048;
    subsubdict["sub_4"] = Dictionary();
    subdict["sub_3"] = std::move(subsubdict);
    dict["sub_2"] = std::move(subdict);
    const auto flattened_dict = dict.getFlattened();
    REQUIRE(flattened_dict.size() == 4);
    REQUIRE(flattened_dict.at("sub_1").get<Dictionary>().empty());
    REQUIRE_FALSE(flattened_dict.contains("sub_2"));
    REQUIRE(flattened_dict.at("sub_2.int").get<int>() == 1024);
    REQUIRE(flattened_dict.at("sub_2.sub_3.int").get<int>() == 2048);
    REQUIRE(flattened_dict.at("sub_2.sub_3.sub_4").get<Dictionary>().empty());
}

TEST_CASE("Dictionary string conversion", "[core][core::config]") {
    Dictionary dict {};
    REQUIRE_THAT(dict.to_string(), Equals("{}"));
    dict["bool"] = true;
    dict["int"] = 1234;
    dict["float"] = 1.5;
    dict["string"] = "hello world";
    dict["array"] = Array({1, 2});
    Dictionary subdict {};
    subdict["nested"] = true;
    dict["dict"] = std::move(subdict);
    REQUIRE_THAT(
        dict.to_string(),
        Equals("{ array: [ 1, 2 ], bool: true, dict: { nested: true }, float: 1.5, int: 1234, string: hello world }"));
}

TEST_CASE("Dictionary format", "[core][core::config]") {
    Dictionary dict {};
    REQUIRE(dict.format(true).empty());
    dict["bool"] = true;
    REQUIRE_THAT(dict.format(true), Equals("\n  bool: true"));
    dict["int"] = 1234;
    dict["float"] = 1.5;
    dict["string"] = "hello world";
    dict["array"] = Array({1, 2});
    dict["filtered"] = 42;
    Dictionary subdict {};
    subdict["nested"] = true;
    subdict["empty_dict"] = Dictionary();
    dict["dict"] = std::move(subdict);
    REQUIRE_THAT(dict.format(
                     false, [](std::string_view key) { return key != "filtered"; }, 0),
                 Equals("array: [ 1, 2 ]\n"
                        "bool: true\n"
                        "dict:\n"
                        "  empty_dict:\n"
                        "  nested: true\n"
                        "float: 1.5\n"
                        "int: 1234\n"
                        "string: hello world"));
}

TEST_CASE("Dictionary type demangling", "[core][core::config]") {
    const Dictionary dict {};
    REQUIRE_THAT(dict.demangle(), Equals("Dictionary"));
}

TEST_CASE("Dictionary msgpack packing & unpacking", "[core][core::config]") {
    // Empty
    Dictionary dict {};
    msgpack::sbuffer sbuffer_empty {};
    msgpack_pack(sbuffer_empty, dict);
    const auto dict_empty = msgpack_unpack_to<Dictionary>(sbuffer_empty.data(), sbuffer_empty.size());
    REQUIRE(dict == dict_empty);
    // Some content
    dict["bool"] = true;
    dict["int"] = 1234;
    dict["float"] = 1.5;
    dict["string"] = "hello world";
    Dictionary subdict {};
    subdict["nested"] = true;
    subdict["array"] = Array({1, 2, 3, 4, 5});
    dict["dict"] = std::move(subdict);
    msgpack::sbuffer sbuffer_content {};
    msgpack_pack(sbuffer_content, dict);
    const auto dict_content = msgpack_unpack_to<Dictionary>(sbuffer_content.data(), sbuffer_content.size());
    REQUIRE(dict == dict_content);
    // Not a map
    msgpack::sbuffer sbuffer_notmap {};
    msgpack_pack(sbuffer_notmap, std::vector<int>({1, 2, 3, 4, 5}));
    REQUIRE_THROWS_AS(msgpack_unpack_to<Dictionary>(sbuffer_notmap.data(), sbuffer_notmap.size()), MsgpackUnpackError);
    // Keys not strings
    msgpack::sbuffer sbuffer_nonstringkeys {};
    msgpack_pack(sbuffer_nonstringkeys, std::map<int, int>({{1, 1}, {2, 4}, {3, 9}, {4, 16}}));
    REQUIRE_THROWS_AS(msgpack_unpack_to<Dictionary>(sbuffer_nonstringkeys.data(), sbuffer_nonstringkeys.size()),
                      MsgpackUnpackError);
    // Unsupported value type
    msgpack::sbuffer sbuffer_unsupported {};
    const std::vector<std::vector<int>> nested_vec = {{1, 2}, {3, 4}, {5, 6}};
    msgpack_pack(sbuffer_unsupported, std::map<std::string, std::vector<std::vector<int>>>({{"nested_vec", nested_vec}}));
    REQUIRE_THROWS_AS(msgpack_unpack_to<Array>(sbuffer_unsupported.data(), sbuffer_unsupported.size()), MsgpackUnpackError);
}

TEST_CASE("Dictionary message assembly & disassembly", "[core][core::config]") {
    Dictionary dict {};
    dict["bool"] = true;
    dict["int"] = 1234;
    dict["float"] = 1.5;
    dict["string"] = "hello world";
    Dictionary subdict {};
    subdict["nested"] = true;
    subdict["array"] = Array({1, 2, 3, 4, 5});
    dict["dict"] = std::move(subdict);
    const auto message = dict.assemble();
    REQUIRE(dict == Dictionary::disassemble(message));
}

// --- Composite ---

TEST_CASE("Composite direct constructors", "[core][core::config]") {
    const Scalar scalar_v {1234};
    const Composite composite_scalar {scalar_v};
    REQUIRE(composite_scalar.get<Scalar>() == scalar_v);
    const Array array_v {1, 2, 3, 4};
    const Composite composite_array {array_v};
    REQUIRE(composite_array.get<Array>() == array_v);
    const Dictionary dict_v {{"hello", "world"}};
    const Composite composite_dict {dict_v};
    REQUIRE(composite_dict.get<Dictionary>() == dict_v);
}

TEST_CASE("Composite indirect constructors and getters", "[core][core::config]") {
    // Scalar
    constexpr bool bool_v = true;
    const Composite composite_scalar_bool {bool_v};
    REQUIRE(composite_scalar_bool.get<bool>() == bool_v);
    constexpr int int_v = 8192;
    const Composite composite_scalar_int {int_v};
    REQUIRE(composite_scalar_int.get<int>() == int_v);
    constexpr double double_v = 1.5;
    const Composite composite_scalar_double {double_v};
    REQUIRE(composite_scalar_double.get<double>() == double_v);
    const std::string string_v = "hello world";
    const Composite composite_scalar_string {string_v};
    REQUIRE(composite_scalar_string.get<std::string>() == string_v);
    // Array
    const std::vector<int> array_int_v {1, 2, 3, 4, 5};
    const Composite composite_array_int {array_int_v};
    REQUIRE_THAT(composite_array_int.get<std::vector<int>>(), RangeEquals(array_int_v));
    const std::array<std::string, 3> array_string_v {"hello", "world"};
    const Composite composite_array_string {array_string_v};
    REQUIRE_THAT(composite_array_string.get<std::vector<std::string>>(), RangeEquals(array_string_v));
    // Dictionary
    const std::map<std::string, int> map_v {{"A", 1}, {"B", 2}, {"C", 3}};
    const Composite composite_map {map_v};
    using map_t = std::map<std::string, int>; // Required due to macro unable to handle comma in type
    REQUIRE_THAT(composite_map.get<map_t>(), RangeEquals(map_v));
    // Coverage for get using variant std::get
    REQUIRE(std::holds_alternative<std::monostate>(Composite().get<Scalar>()));
}

TEST_CASE("Composite default constructor", "[core][core::config]") {
    const auto composite_valueless = Composite();
    // By default valueless scalar
    REQUIRE(composite_valueless.get<Scalar>() == Scalar());
}

TEST_CASE("Composite operators", "[core][core::config]") {
    Composite composite {};
    REQUIRE_FALSE(composite == std::vector<bool>({false}));
    composite = 1.5F;
    REQUIRE(composite == 1.5F);
    REQUIRE_FALSE(composite == "hello world");
    composite = Array({3.0F, 4.0F});
    REQUIRE(composite == std::vector<float>({3.0F, 4.0F}));
    REQUIRE_FALSE(composite == 3.0F);
    composite = Dictionary({{"hello", 1}, {"world", 2}});
    REQUIRE(composite == std::map<std::string, int>({{"hello", 1}, {"world", 2}}));
    REQUIRE_FALSE(composite == std::vector<std::string>({"hello", "world"}));
}

TEST_CASE("Composite string conversion", "[core][core::config]") {
    const Composite composite_scalar {1.5};
    REQUIRE_THAT(composite_scalar.to_string(), Equals("1.5"));
    const Composite composite_array {Array({1, 2, 3})};
    REQUIRE_THAT(composite_array.to_string(), Equals("[ 1, 2, 3 ]"));
    const Composite composite_dictionary {Dictionary({{"hello", 1}, {"world", 2}})};
    REQUIRE_THAT(composite_dictionary.to_string(), Equals("{ hello: 1, world: 2 }"));
}

TEST_CASE("Composite type demangling", "[core][core::config]") {
    const Composite composite_scalar {1.5};
    REQUIRE_THAT(composite_scalar.demangle(), Equals("double"));
    const Composite composite_array {Array({true, false, true})};
    REQUIRE_THAT(composite_array.demangle(), Equals("Array<bool>"));
    const Composite composite_dictionary {Dictionary({{"hello", 1}, {"world", 2}})};
    REQUIRE_THAT(composite_dictionary.demangle(), Equals("Dictionary"));
}

TEST_CASE("Composite msgpack packing & unpacking", "[core][core::config]") {
    Composite composite {};
    composite = "hello world";
    msgpack::sbuffer sbuffer_scalar {};
    msgpack_pack(sbuffer_scalar, composite);
    const auto composite_scalar = msgpack_unpack_to<Composite>(sbuffer_scalar.data(), sbuffer_scalar.size());
    REQUIRE(composite == composite_scalar);
    composite = Array({1, 2, 3, 4, 5});
    msgpack::sbuffer sbuffer_array {};
    msgpack_pack(sbuffer_array, composite);
    const auto composite_array = msgpack_unpack_to<Composite>(sbuffer_array.data(), sbuffer_array.size());
    REQUIRE(composite == composite_array);
    composite = Dictionary({{"hello", 1}, {"world", 2}});
    msgpack::sbuffer sbuffer_dict {};
    msgpack_pack(sbuffer_dict, composite);
    const auto composite_dict = msgpack_unpack_to<Composite>(sbuffer_dict.data(), sbuffer_dict.size());
    REQUIRE(composite == composite_dict);
}

TEST_CASE("Composite message assembly & disassembly", "[core][core::config]") {
    Composite composite {};
    Dictionary dict {};
    dict["hello"] = "world";
    composite = std::move(dict);
    const auto message = composite.assemble();
    REQUIRE(composite == Composite::disassemble(message));
}

// --- Composite List ---

TEST_CASE("CompositeList range constructor", "[core][core::config]") {
    const CompositeList composite_list {std::vector<std::string>({"hello", "world"})};
    REQUIRE_THAT(composite_list.at(0).get<std::string>(), Equals("hello"));
    REQUIRE_THAT(composite_list.at(1).get<std::string>(), Equals("world"));
}

TEST_CASE("CompositeList default constructor", "[core][core::config]") {
    const auto composite_list_valueless = CompositeList();
    REQUIRE(composite_list_valueless.empty());
}

TEST_CASE("CompositeList inhomogenity", "[core][core::config]") {
    CompositeList composite_list {};
    composite_list.emplace_back("set_channel_properties");
    composite_list.emplace_back(1);
    composite_list.emplace_back(Array({1.5, 10.0, 0.110}));
    REQUIRE_THAT(composite_list.at(0).get<std::string>(), Equals("set_channel_properties"));
    REQUIRE(composite_list.at(1).get<int>() == 1);
    REQUIRE_THAT(composite_list.at(2).get<std::vector<double>>(), RangeEquals(std::vector<double>({1.5, 10.0, 0.110})));
}

TEST_CASE("CompositeList string conversion", "[core][core::config]") {
    CompositeList composite_list {};
    REQUIRE_THAT(composite_list.to_string(), Equals("[]"));
    composite_list.emplace_back("test");
    composite_list.emplace_back(1.5F);
    REQUIRE_THAT(composite_list.to_string(), Equals("[ test, 1.5 ]"));
}

TEST_CASE("CompositeList msgpack packing & unpacking", "[core][core::config]") {
    // Empty
    CompositeList composite_list {};
    msgpack::sbuffer sbuffer_empty {};
    msgpack_pack(sbuffer_empty, composite_list);
    const auto composite_list_empty = msgpack_unpack_to<CompositeList>(sbuffer_empty.data(), sbuffer_empty.size());
    REQUIRE(composite_list == composite_list_empty);
    // Some content
    composite_list.emplace_back("set_channel_properties");
    composite_list.emplace_back(1);
    composite_list.emplace_back(Array({1.5, 10.0, 0.110}));
    msgpack::sbuffer sbuffer_content {};
    msgpack_pack(sbuffer_content, composite_list);
    const auto composite_list_content = msgpack_unpack_to<CompositeList>(sbuffer_content.data(), sbuffer_content.size());
    REQUIRE(composite_list == composite_list_content);
    // Not an array
    msgpack::sbuffer sbuffer_notarray {};
    msgpack_pack(sbuffer_notarray, "hello world");
    REQUIRE_THROWS_AS(msgpack_unpack_to<CompositeList>(sbuffer_notarray.data(), sbuffer_notarray.size()),
                      MsgpackUnpackError);
    // Unsupported value type
    msgpack::sbuffer sbuffer_unsupported {};
    const std::vector<std::vector<int>> nested_vec = {{1, 2}, {3, 4}, {5, 6}};
    msgpack_pack(sbuffer_unsupported, std::map<std::string, std::vector<std::vector<int>>>({{"nested_vec", nested_vec}}));
    REQUIRE_THROWS_AS(msgpack_unpack_to<CompositeList>(sbuffer_unsupported.data(), sbuffer_unsupported.size()),
                      MsgpackUnpackError);
}

TEST_CASE("CompositeList message assembly & disassembly", "[core][core::config]") {
    CompositeList composite_list {};
    composite_list.emplace_back("set_channel_to");
    composite_list.emplace_back(1);
    composite_list.emplace_back(5.0);
    const auto message = composite_list.assemble();
    REQUIRE(composite_list == CompositeList::disassemble(message));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
