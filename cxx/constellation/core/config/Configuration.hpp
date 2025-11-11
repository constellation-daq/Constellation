/**
 * @file
 * @brief Configuration
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"

namespace constellation::config {

    /**
     * @brief Class to access a section in the configuration
     *
     * Each `Section` corresponds to a `Dictionary`. It provides convenient access methods for the `Dictionary`,
     * keeps track of used and unused values, and owns any nested `Section` classes corresponding to `Dictionary`
     * classes contained in the `Dictionary` class corresponding to itself.
     */
    class Section {
    public:
        enum class ConfigurationGroup : std::uint8_t {
            /** All configuration key-value pairs, both user and internal */
            ALL,
            /** Configuration key-value pairs intended for framework users */
            USER,
            /** Configuration key-value pairs intended for internal framework usage */
            INTERNAL,
        };
        using enum ConfigurationGroup;

    public:
        /**
         * @brief Construct a new configuration section
         *
         * @param prefix Prefix for the configuration section, e.g. `channel_1.`
         * @param dictionary Pointer to the corresponding `Dictionary` object
         * @throws InvalidKeyError If two keys with the same lowercase spellings are contained in the dictionary
         */
        CNSTLN_API Section(std::string prefix, Dictionary* dictionary);

        /** Destructor */
        virtual ~Section() = default;

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        Section(const Section& other) = delete;
        Section& operator=(const Section& other) = delete;
        Section(Section&& other) = delete;
        Section& operator=(Section&& other) = delete;
        /// @endcond

    private:
        // Configuration needs access to internal variables for move operations
        friend class Configuration;

    public:
        /**
         * @brief Check if key is defined
         *
         * @param key Key to check for existence
         * @return True if key exists, false otherwise
         */
        CNSTLN_API bool has(std::string_view key) const;

        /**
         * @brief Check how many of the given keys are defined
         *
         * This is useful to check if two or more conflicting configuration keys that are defined.
         *
         * @param keys Keys to check for existence
         * @return Number of existing keys from the given list
         */
        CNSTLN_API std::size_t count(std::initializer_list<std::string_view> keys) const;

        /**
         * @brief Set default value for a key only if it is not defined yet
         *
         * @note This does not mark the key as used.
         *
         * @param key Key to possible set value of
         * @param default_value Value to assign if the key is not defined yet
         */
        template <typename T> void setDefault(std::string_view key, T&& default_value) const;

        /**
         * @brief Set alias name for an already existing key
         *
         * @note This marks the old key as "used" automatically.
         *
         * @param new_key New alias to be created
         * @param old_key Key the alias is created for
         * @param warn Optionally print a warning message to notify of deprecation
         */
        CNSTLN_API void setAlias(std::string_view new_key, std::string_view old_key, bool warn = true) const;

        /**
         * @brief Get value of a key in requested type
         *
         * @param key Key to get value of
         * @return Value of the key in the type of the requested template parameter
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> T get(std::string_view key) const;

        /**
         * @brief Get value of a key in requested type or default value if it does not exists
         *
         * @param key Key to get value of
         * @param default_value Default value to set if key is not defined
         * @return Value of the key in the type of the requested template parameter
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> T get(std::string_view key, T default_value) const;

        /**
         * @brief Get an optional with value of a key in requested type if available
         *
         * @param key Key to get value of
         * @return Optional holding the value of the key in the type of the requested template parameter if available
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::optional<T> getOptional(std::string_view key) const;

        /**
         * @brief Get values for a key containing an array
         *
         * @note This will also attempt to read the configuration key as single value and will return a vector
         *       with one entry if succeeding.
         *
         * @param key Key to get values of
         * @return List of values in the array in the requested template parameter
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::vector<T> getArray(std::string_view key) const;

        /**
         * @brief Get values for a key containing an array or a default array if it does not exists
         *
         * @note This will also attempt to read the configuration key as single value and will return a vector
         *       with one entry if succeeding.
         *
         * @param key Key to get values of
         * @param default_value Default value to set if key is not defined
         * @return List of values in the array in the requested template parameter
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::vector<T> getArray(std::string_view key, std::vector<T> default_value) const;

        /**
         * @brief Get an optional with the values for a key containing an array if available
         *
         * @note This will also attempt to read the configuration key as single value and will return a vector
         *       with one entry if succeeding.
         *
         * @param key Key to get values of
         * @return Optional with a list of values in the array in the requested template parameter if available
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::optional<std::vector<T>> getOptionalArray(std::string_view key) const;

        /**
         * @brief Get values for a key containing a set
         *
         * @note This will also attempt to read the configuration key as single value and will return a set
         *       with one entry if succeeding.
         *
         * @param key Key to get values of
         * @return Set of values in the requested template parameter
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::set<T> getSet(std::string_view key) const;

        /**
         * @brief Get values for a key containing a set or default set if it does not exists
         *
         * @note This will also attempt to read the configuration key as single value and will return a set
         *       with one entry if succeeding.
         *
         * @param key Key to get values of
         * @param default_value Default value to set if key is not defined
         * @return Set of values in the requested template parameter
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::set<T> getSet(std::string_view key, const std::set<T>& default_value) const;

        /**
         * @brief Get an optional with values for a key containing a set if available
         *
         * @note This will also attempt to read the configuration key as single value and will return a set
         *       with one entry if succeeding.
         *
         * @param key Key to get values of
         * @return Optional with a set of values in the requested template parameter if available
         * @throws InvalidTypeError If the value could not be cast to desired type
         * @throws InvalidValueError If the value is not valid for the requested type
         */
        template <typename T> std::optional<std::set<T>> getOptionalSet(std::string_view key) const;

        /**
         * @brief Get path
         *
         * @note Relative paths are taken relative to the current work directory.
         *
         * @param key Key to get path of
         * @param check_exists If the file should be checked for existence (if true always returns a canonical path)
         * @return Absolute path
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the value is not a string
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        CNSTLN_API std::filesystem::path getPath(std::string_view key, bool check_exists = false) const;

        /**
         * @brief Get list of paths
         *
         * @note Relative paths are taken relative to the current work directory.
         *
         * @param key Key to get list of path of
         * @param check_exists If the file should be checked for existence (if true always returns a canonical path)
         * @return List of absolute paths
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the value is not a string
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        CNSTLN_API std::vector<std::filesystem::path> getPathArray(std::string_view key, bool check_exists = false) const;

        /**
         * @brief Get nested configuration section
         *
         * @param key Key to get section of
         * @return Configuration section contained by the key
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the value is not a section
         */
        CNSTLN_API const Section& getSection(std::string_view key) const;

        /**
         * @brief Get the keys of the configuration section
         *
         * @note This does not mark the keys as used.
         *
         * @return List containing the keys of the section
         */
        CNSTLN_API std::vector<std::string> getKeys() const;

        /**
         * @brief Get literal value of a key as string
         *
         * @warning This does not mark the key as used, and thus should never be used to retrieve a configuration value.
         *
         * @param key Key to get values of
         * @return Literal value of the key
         */
        CNSTLN_API std::string getText(std::string_view key) const;

        /**
         * @brief Get configuration as dictionary
         *
         * @warning Accessing entries in the dictionary does not mark the key as used,
         *          and thus should never be used to retrieve configuration values.
         *
         * @return Dictionary containing the configuration section
         */
        const Dictionary& asDictionary() const { return *dictionary_; }

        /**
         * @brief Check if the configuration section is empty
         *
         * @return True if section is empty, false otherwise
         */
        bool empty() const { return dictionary_->empty(); }

        /**
         * @brief Convert configuration section to a string
         *
         * @param configuration_group Group of configuration key-value pairs to include
         * @return String containing the configuration section
         */
        CNSTLN_API std::string to_string(ConfigurationGroup configuration_group = ALL) const;

        /**
         * @brief Remove unused entries from the configuration section
         *
         * @return List of unused keys
         */
        CNSTLN_API std::vector<std::string> removeUnusedEntries();

        /**
         * @brief Update configuration with values from another configuration section
         *
         * @details Before updating this method validates that all keys which are to be updated already exist and that their
         *          corresponding values have the same type after the update.
         *
         * @param other Configuration section from which to update values from
         */
        CNSTLN_API void update(const Section& other);

    private:
        CNSTLN_LOCAL void convert_lowercase();
        CNSTLN_LOCAL void create_section_tree();
        CNSTLN_LOCAL void mark_used(std::string_view key) const { used_keys_.emplace(key); }
        CNSTLN_LOCAL void update_impl(const Section& other);

    private:
        std::string prefix_;
        Dictionary* dictionary_;
        mutable std::set<std::string> used_keys_;
        mutable std::map<std::string, Section> section_tree_;
    };

    /**
     * @brief Class which owns the root dictionary of the configuration
     *
     * This class is required since the `Configuration` class cannot own the root dictionary due to initialization order.
     */
    class RootDictionaryHolder {
    public:
        /**
         * @brief Construct with an empty root dictionary
         */
        RootDictionaryHolder() = default;

        /**
         * @brief Construct from an existing dictionary
         *
         * @note This constructor iterates recursively over all keys and converts them to lower-case.
         */
        RootDictionaryHolder(Dictionary dictionary);

    private:
        // Configuration needs access to internal variables for move operations
        friend class Configuration;

    private:
        Dictionary root_dictionary_;
    };

    /**
     * @brief Class for the top-level configuration of a satellite
     *
     * This class is is a `Section` with additional methods to make suitable to be used within the framework.
     * It inherits from `RootDictionaryHolder` since constructors first initializes all base classes and then
     * member variables, meaning it cannot own the root dictionary as member to initialize the base `Section`.
     */
    class Configuration : private RootDictionaryHolder, public Section {
    public:
        /**
         * @brief Construct an empty configuration
         */
        CNSTLN_API Configuration();

        virtual ~Configuration() = default;

        /// @cond doxygen_suppress
        // No copy constructor/assignment
        Configuration(const Configuration& other) = delete;
        Configuration& operator=(const Configuration& other) = delete;
        /// @endcond

        /**
         * @brief Construct a configuration from a dictionary
         *
         * @param root_dictionary Root dictionary of the configuration
         */
        CNSTLN_API Configuration(Dictionary root_dictionary);

        /** Move constructor */
        CNSTLN_API Configuration(Configuration&& other) noexcept;

        /** Move assignment */
        CNSTLN_API Configuration& operator=(Configuration&& other) noexcept;

        /** Swap function */
        void swap(Configuration& other) noexcept;

        // TODO: as flat dictionary? useful e.g. for Qt printing

        /** Assemble via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble from message payload */
        CNSTLN_API static Configuration disassemble(const message::PayloadBuffer& message);
    };

} // namespace constellation::config

// Include template members
#include "Configuration.ipp" // IWYU pragma: keep
