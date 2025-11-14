/**
 * @file
 * @brief Configuration value types with serialization functions for MessagePack
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <msgpack/object_decl.hpp>
#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/build.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::config {

    // --- Helpers ---

    /**
     * @brief Helper to cast string value to enum
     *
     * @param value String with enum value name
     * @return Enum in the requested type
     * @throws std::invalid_argument If the string value is not a valid enum value name
     */
    template <typename E>
        requires std::is_enum_v<E>
    E config_enum_cast(std::string_view value);

    /**
     * @brief Helper to cast one integer to another
     *
     * @param value Integer value to cast
     * @return Integer in the requested type
     * @throws std::invalid_argument If the integer value is out of range for the requested type
     */
    template <typename T, typename U> T config_numeric_cast(U value);

    // --- Scalar ---

    // Variant types for Scalar
    using ScalarVariant = std::variant<bool, std::int64_t, double, std::string, std::chrono::system_clock::time_point>;
    using ScalarMonostateVariant = utils::monostate_variant_t<ScalarVariant>;

    // Concept for scalar values
    template <typename T>
    concept scalar = utils::is_one_of_v<T, ScalarVariant> || std::integral<T> || std::floating_point<T> ||
                     std::same_as<T, std::string_view> || std::is_enum_v<T>;

    // Concept for scalar construction
    template <typename T>
    concept scalar_constructible = scalar<T> || std::convertible_to<T, std::string_view>;

    /**
     * @brief Scalar (non-nestable) value
     */
    class Scalar : public ScalarMonostateVariant {
    public:
        /**
         * @brief Construct a new valueless scalar
         */
        Scalar() = default;

        /**
         * @brief Construct a new scalar
         *
         * @param value Value to be set for the new scalar
         */
        template <typename T>
            requires scalar_constructible<T>
        Scalar(T value);

        /** Assignment operator */
        template <typename T>
            requires scalar_constructible<T>
        Scalar& operator=(T other);

        /** Equality operator */
        template <typename T>
            requires scalar_constructible<T>
        bool operator==(T other) const;

        /**
         * @brief Get scalar in requested type
         *
         * @return Value in the type of the requested template parameter
         * @throws bad_variant_access If the scalar could not be cast to requested type
         * @throws invalid_argument If the scalar value is not valid for the requested type
         */
        template <typename T>
            requires scalar<T>
        T get() const;

        /**
         * @brief Convert scalar to string representation
         *
         * @return String representation of the value
         */
        CNSTLN_API std::string to_string() const;

        /**
         * @brief Demangle type held by the scalar
         *
         * @return Demangled name of the currently held type
         */
        CNSTLN_API std::string demangle() const;

        /** Pack with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);
    };

    // --- Array ---

    // Variant types for Array
    using ArrayVariant = utils::vector_variant_t<ScalarVariant>;
    using ArrayMonostateVariant = utils::monostate_variant_t<ArrayVariant>;

    // Concept for array construction
    template <typename R>
    concept array_constructible = std::ranges::forward_range<R> && scalar_constructible<std::ranges::range_value_t<R>>;

    /**
     * @brief Array of scalar values
     */
    class Array : public ArrayMonostateVariant {
    public:
        /**
         * @brief Construct a new empty array
         */
        Array() = default;

        /**
         * @brief Construct a new array from an initializer list
         *
         * @param init Initializer list for array
         */
        template <typename T>
            requires scalar_constructible<T>
        Array(std::initializer_list<T> init);

        /**
         * @brief Construct a new array from a range
         *
         * @param range Range to be set for the new array
         */
        template <typename R>
            requires array_constructible<R>
        Array(const R& range);

        /** Assignment operator */
        template <typename R>
            requires array_constructible<R>
        Array& operator=(const R& other);

        /** Equality operator */
        template <typename R>
            requires array_constructible<R>
        bool operator==(const R& other) const;

        /**
         * @brief Get array as std::vector in requested type
         *
         * @return Vector with elements in the type of the requested template parameter
         * @throws bad_variant_access If the array could not be cast to a vector of the requested type
         * @throws invalid_argument If at least one of the array elements is not valid for the requested type
         */
        template <typename T>
            requires scalar<T>
        std::vector<T> getVector() const;

        /**
         * @brief Check if the array is empty
         *
         * @return True if the array is empty, false otherwise
         */
        bool empty() const { return std::holds_alternative<std::monostate>(*this); }

        /**
         * @brief Convert array to string representation
         *
         * @return String representation of the array
         */
        CNSTLN_API std::string to_string() const;

        /**
         * @brief Demangle type held by the array
         *
         * @return Demangled name of the currently held type
         */
        CNSTLN_API std::string demangle() const;

        /** Pack with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

    private:
        template <typename T, typename R, typename F> static std::vector<T> static_transform(const R& range, F&& op);
        template <typename T, typename U, typename F> std::vector<T> transform(F&& op) const;
    };

    // --- Dictionary ---

    // Forward declaration of composite
    class Composite;

    /**
     * @brief Dictionary which maps strings to a composite
     */
    class Dictionary : public std::map<std::string, Composite> {
    public:
        /**
         * @brief Construct a new empty dictionary
         */
        Dictionary() = default;

        // Inherit constructors from std::map
        using std::map<std::string, Composite>::map;

        /**
         * @brief Construct a new dictionary from an std::map
         *
         * @param map Map to be set for the new dictionary
         */
        template <typename T>
            requires(!std::same_as<T, Composite>)
        Dictionary(const std::map<std::string, T>& map);

        /** Assignment operator */
        template <typename T>
            requires(!std::same_as<T, Composite>)
        Dictionary& operator=(const std::map<std::string, T>& other);

        /** Equality operator */
        template <typename T>
            requires(!std::same_as<T, Composite>)
        bool operator==(const std::map<std::string, T>& other) const;

        /**
         * @brief Get dictionary as std::map with values in requested type
         *
         * @note This is only possible if the dictionary is homogeneous and flat.
         *
         * @return Map with value in the type of the requested template parameter
         * @throws bad_variant_access If the dictionary could not be cast to an std::map with values of the requested type
         * @throws invalid_argument If at least one of the dictionary values is not valid for the requested type
         */
        template <typename T> std::map<std::string, T> getMap() const;

        /**
         * @brief Convert dictionary to string representation
         *
         * @return String representation of the dictionary
         */
        CNSTLN_API std::string to_string() const;

        // Key filter function signature (if return value is true then key is accepted)
        using key_filter = bool(std::string_view key);

        /**
         * @brief Default key filter accepting all keys
         *
         * @return True for all keys
         */
        static bool default_key_filter(std::string_view /*key*/) { return true; } // NOLINT(readability-identifier-naming)

        /**
         * @brief Format dictionary to YAML-style string
         *
         * @param newline_prefix If the string should be prefix with a newline if not empty
         * @param filter Key filter function to only include certain keys
         * @param indent Indent to prefix keys with (always increased by 2 for nested dictionaries)
         * @return String representation of the dictionary in YAML-style
         */
        CNSTLN_API std::string format(bool newline_prefix,
                                      key_filter* filter = default_key_filter,
                                      std::size_t indent = 2) const;

        /**
         * @brief Demangle type
         *
         * @return Demangled name of the type
         */
        CNSTLN_API std::string demangle() const;

        /** Pack with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

        /** Assemble via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble from message payload */
        CNSTLN_API static Dictionary disassemble(const message::PayloadBuffer& message);
    };

    // --- Composite ---

    // Variant type for Composite
    using CompositeVariant = std::variant<Scalar, Array, Dictionary>;

    // Concept for composite construction
    template <typename T>
    concept composite_constructible = (std::constructible_from<Scalar, T> || std::constructible_from<Array, T> ||
                                       std::constructible_from<Dictionary, T>) &&
                                      (!utils::is_one_of_v<T, CompositeVariant>);

    /**
     * @brief Composite which is either scalar, array, or dictionary
     */
    class Composite : public CompositeVariant {
    public:
        /**
         * @brief Construct a new valueless composite
         */
        Composite() = default;

        /**
         * @brief Construct a new composite from underlying variant type
         *
         * @param value Value to be set for the new composite
         */
        template <typename T>
            requires utils::is_one_of_v<T, CompositeVariant>
        Composite(T value);

        /**
         * @brief Construct a new composite
         *
         * @param value Value to be set for the new composite
         */
        template <typename T>
            requires composite_constructible<T>
        Composite(const T& value);

        /** Assignment operator */
        template <typename T>
            requires composite_constructible<T>
        Composite& operator=(const T& other);

        /** Equality operator */
        template <typename T>
            requires composite_constructible<T>
        bool operator==(const T& other) const;

        /**
         * @brief Get composite in requested type
         *
         * @return Value in the type of the requested template parameter
         * @throws bad_variant_access If the composite could not be cast to requested type
         * @throws invalid_argument If the composite value is not valid for the requested type
         */
        template <typename T>
            requires(!utils::is_one_of_v<T, CompositeVariant>)
        T get() const;

        /**
         * @brief Get constant reference to underlying type
         *
         * @return Constant reference to the value in the type of the requested template parameter
         * @throws bad_variant_access If the composite could not be cast to requested type
         */
        template <typename T>
            requires(utils::is_one_of_v<T, CompositeVariant>)
        const T& get() const;

        /**
         * @brief Get reference to underlying type
         *
         * @return Reference to the value in the type of the requested template parameter
         * @throws bad_variant_access If the composite could not be cast to requested type
         */
        template <typename T>
            requires(utils::is_one_of_v<T, CompositeVariant>)
        T& get();

        /**
         * @brief Convert composite to string representation
         *
         * @return String representation of the value
         */
        CNSTLN_API std::string to_string() const;

        /**
         * @brief Demangle type held by the composite
         *
         * @return Demangled name of the currently held type
         */
        CNSTLN_API std::string demangle() const;

        /** Pack with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

        /** Assemble via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble from message payload */
        CNSTLN_API static Composite disassemble(const message::PayloadBuffer& message);
    };

    // --- Composite List ---

    /**
     * @brief List of composites
     */
    class CompositeList : public std::vector<Composite> {
    public:
        /**
         * @brief Construct a new empty composite list
         */
        CompositeList() = default;

        /**
         * @brief Construct a new composite list from a range
         *
         * @param range Range to be set for the new composite list
         */
        template <typename R>
            requires std::ranges::forward_range<R> && std::constructible_from<Composite, std::ranges::range_value_t<R>>
        CompositeList(const R& range);

        /**
         * @brief Convert composite list to string representation
         *
         * @return String representation of the composite list
         */
        CNSTLN_API std::string to_string() const;

        /** Pack with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

        /** Assemble via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble from message payload */
        CNSTLN_API static CompositeList disassemble(const message::PayloadBuffer& message);
    };

} // namespace constellation::config

// Include template members
#include "value_types.ipp" // IWYU pragma: keep
