/**
 * @file
 * @brief Implementation of configuration
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Configuration.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;

// NOLINTBEGIN(misc-no-recursion)
Section::Section(std::string prefix, Dictionary* dictionary) : prefix_(std::move(prefix)), dictionary_(dictionary) {
    // Convert dictionary to lowercase
    convert_lowercase();
    // Create configuration section for any nested dictionaries in the dictionary
    create_section_tree();
}
// NOLINTEND(misc-no-recursion)

void Section::convert_lowercase() {
    // Iterate over nodes
    auto it = dictionary_->begin();
    while(it != dictionary_->end()) {
        // Convert key to lowercase and check if different
        const auto& key = it->first;
        const auto key_lc = transform(key, ::tolower);
        if(key_lc != key) {
            // Extract node to avoid iterator invalidation
            auto node = dictionary_->extract(it);
            // Change key to lowercase and insert back into map
            node.key() = key_lc;
            const auto insert_res = dictionary_->insert(std::move(node));
            // Check that node was insert, if not there were duplicate keys
            if(!insert_res.inserted) {
                throw InvalidKeyError(*this, key, "key defined twice");
            }
        }
        // Advance iterator
        ++it;
    }
}

// NOLINTBEGIN(misc-no-recursion)
void Section::create_section_tree() {
    for(auto& [key, value] : *dictionary_) {
        // Check for dictionaries
        if(std::holds_alternative<Dictionary>(value)) {
            auto& dict = value.get<Dictionary>();

            // Create and store new nested configuration section
            const auto sub_prefix = prefix_ + key + '.';
            section_tree_.try_emplace(key, sub_prefix, &dict);
        }
    }
}
// NOLINTEND(misc-no-recursion)

bool Section::has(std::string_view key) const {
    const auto key_lc = transform(key, ::tolower);
    return dictionary_->contains(key_lc);
}

std::size_t Section::count(std::initializer_list<std::string_view> keys) const {
    if(keys.size() == 0) {
        throw std::invalid_argument("list of keys to count cannot be empty");
    }

    std::size_t found = 0;
    for(const auto& key : keys) {
        if(has(key)) {
            ++found;
        }
    }
    return found;
}

void Section::setAlias(std::string_view new_key, std::string_view old_key, bool warn) const {
    if(!has(old_key)) {
        return;
    }

    const auto new_key_lc = transform(new_key, ::tolower);
    const auto old_key_lc = transform(old_key, ::tolower);
    (*dictionary_)[new_key_lc] = std::move(dictionary_->at(old_key_lc));
    dictionary_->erase(old_key_lc);

    LOG_IF(WARNING, warn) << "Parameter " << quote(old_key) << " is deprecated and superseded by " << quote(new_key);
}

namespace {
    std::filesystem::path path_to_absolute(std::filesystem::path path, bool check_exists) {
        // If not a absolute path, make it an absolute path
        if(!path.is_absolute()) {
            // Get current directory and append the relative path
            path = std::filesystem::current_path() / path;
        }

        // Check if the path exists
        const auto exists = std::filesystem::exists(path);
        if(check_exists && !exists) {
            throw std::invalid_argument("path " + quote(path.string()) + " not found");
        }

        // Normalize path if the path exists
        if(exists) {
            path = std::filesystem::canonical(path);
        }

        return path;
    }
} // namespace

std::filesystem::path Section::getPath(std::string_view key, bool check_exists) const {
    const auto path_str = get<std::string>(key);
    try {
        return path_to_absolute(path_str, check_exists);
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(*this, key, e.what());
    }
}

std::vector<std::filesystem::path> Section::getPathArray(std::string_view key, bool check_exists) const {
    std::vector<std::filesystem::path> out {};
    const auto path_str_arr = getArray<std::string>(key);
    out.reserve(path_str_arr.size());
    for(const auto& path_str : path_str_arr) {
        try {
            out.emplace_back(path_to_absolute(path_str, check_exists));
        } catch(std::invalid_argument& e) {
            throw InvalidValueError(*this, key, e.what());
        }
    }
    return out;
}

const Section& Section::getSection(std::string_view key) const {
    const auto key_lc = transform(key, ::tolower);

    // Try to get configuration section from tree
    try {
        const auto& section = section_tree_.at(key_lc);
        mark_used(key_lc);
        return section;
    } catch(const std::out_of_range&) {
        const auto it = dictionary_->find(key_lc);
        if(it != dictionary_->cend()) {
            throw InvalidTypeError(*this, key, it->second.demangle(), "Section");
        }
        throw MissingKeyError(*this, key);
    }
}

const Section& Section::getSection(std::string_view key, Dictionary&& default_value) {
    // Set default value manually since dictionary needs to be inserted into tree
    const auto key_lc = utils::transform(key, tolower);
    const auto confdict_it = section_tree_.find(key_lc);
    if(confdict_it == section_tree_.cend()) {
        auto [it, inserted] = dictionary_->try_emplace(key_lc, std::move(default_value));
        if(!inserted) {
            throw InvalidTypeError(*this, key, it->second.demangle(), "Section");
        }
        section_tree_.try_emplace(key_lc, prefix_ + key_lc + ".", &it->second.get<Dictionary>());
    }

    return getSection(key_lc);
}

std::optional<std::reference_wrapper<const Section>> Section::getOptionalSection(std::string_view key) const {
    try {
        return getSection(key);
    } catch(const MissingKeyError&) {
        return std::nullopt;
    }
}

std::vector<std::string> Section::getKeys() const {
    std::vector<std::string> out {};
    std::ranges::for_each(*dictionary_, [&](const auto& p) { out.emplace_back(p.first); });
    return out;
}

std::string Section::getText(std::string_view key) const {
    const auto key_lc = transform(key, ::tolower);
    try {
        return dictionary_->at(key_lc).to_string();
    } catch(const std::out_of_range&) {
        throw MissingKeyError(*this, key);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
std::vector<std::string> Section::removeUnusedEntries() {
    std::vector<std::string> unused_keys {};
    std::vector<std::string> unused_keys_to_remove {};
    for(const auto& [key, value] : *dictionary_) {
        if(std::holds_alternative<Dictionary>(value)) {
            // Sections require special handling
            auto& sub_section = section_tree_.at(key);
            auto sub_unused_keys = sub_section.removeUnusedEntries();
            // Check if section was accessed at all
            if(used_keys_.contains(key)) {
                // If accessed, add unused sub keys recursively
                std::ranges::move(sub_unused_keys, std::back_inserter(unused_keys));
            } else {
                // If unused, add section key instead of sub keys and remove section entirely
                unused_keys.emplace_back(prefix_ + key);
                unused_keys_to_remove.emplace_back(key);
                section_tree_.erase(key);
            }
        } else {
            // Check if unused and store key for removal
            if(!used_keys_.contains(key)) {
                unused_keys.emplace_back(prefix_ + key);
                unused_keys_to_remove.emplace_back(key);
            }
        }
    }
    // Remove unused keys
    std::ranges::for_each(unused_keys_to_remove, [this](const auto& key) { dictionary_->erase(key); });
    return unused_keys;
}

void Section::update(const Section& other) {
    // Validate update beforehand
    validate_update(other);

    // Update values without validating again
    update_impl(other);
}

// NOLINTNEXTLINE(misc-no-recursion)
void Section::validate_update(const Section& other) {
    for(const auto& [other_key, other_value] : *other.dictionary_) {
        // Check that key is also in dictionary
        const auto entry_it = dictionary_->find(other_key);
        if(entry_it == dictionary_->cend()) {
            throw InvalidUpdateError(other, other_key, "key does not exist in current configuration");
        }
        const auto& key = entry_it->first;
        const auto& value = entry_it->second;
        // Check that values has the same type
        if(value.index() != other_value.index()) {
            throw InvalidUpdateError(other,
                                     other_key,
                                     "cannot change type from " + quote(value.demangle()) + " to " +
                                         quote(other_value.demangle()));
        }
        if(std::holds_alternative<Scalar>(value)) {
            const auto& scalar = value.get<Scalar>();
            const auto& other_scalar = other_value.get<Scalar>();
            // Compare scalar type
            if(scalar.index() != other_scalar.index()) {
                throw InvalidUpdateError(other,
                                         other_key,
                                         "cannot change type from " + quote(scalar.demangle()) + " to " +
                                             quote(other_scalar.demangle()));
            }
        } else if(std::holds_alternative<Array>(value)) {
            const auto& array = value.get<Array>();
            const auto& other_array = other_value.get<Array>();
            // If array, check that elements have the same type (if not empty)
            if(!array.empty() && !other_array.empty() && array.index() != other_array.index()) {
                throw InvalidUpdateError(other,
                                         other_key,
                                         "cannot change type from " + quote(array.demangle()) + " to " +
                                             quote(other_array.demangle()));
            }
        } else if(std::holds_alternative<Dictionary>(value)) {
            // If dictionary, validate recursively
            section_tree_.at(key).validate_update(other.section_tree_.at(other_key));
        } else {
            std::unreachable();
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void Section::update_impl(const Section& other) {
    for(const auto& [other_key, other_value] : *other.dictionary_) {
        auto entry_it = dictionary_->find(other_key);
        const auto& key = entry_it->first;
        const auto& value = entry_it->second;
        if(std::holds_alternative<Dictionary>(value)) {
            // If dictionary, update recursively
            section_tree_.at(key).update_impl(other.section_tree_.at(other_key));
        } else {
            entry_it->second = other_value;
        }
    }
}

RootDictionaryHolder::RootDictionaryHolder(Dictionary dictionary) : root_dictionary_(std::move(dictionary)) {}

Configuration::Configuration() : Section("", &root_dictionary_) {}

Configuration::Configuration(Dictionary root_dictionary)
    : RootDictionaryHolder(std::move(root_dictionary)), Section("", &root_dictionary_) {}

// NOLINTNEXTLINE(bugprone-exception-escape): Exception not possible since root dict is empty during section construction
Configuration::Configuration(Configuration&& other) noexcept : Configuration() {
    this->swap(other);
}

Configuration& Configuration::operator=(Configuration&& other) noexcept {
    this->swap(other);
    return *this;
}

void Configuration::swap(Configuration& other) noexcept {
    // Swap root dictionary
    root_dictionary_.swap(other.root_dictionary_);
    // Exchange root dictionaries
    dictionary_ = &root_dictionary_;
    other.dictionary_ = &other.root_dictionary_;
    // Swap prefix, used keys and configuration section tree
    prefix_.swap(other.prefix_);
    used_keys_.swap(other.used_keys_);
    section_tree_.swap(other.section_tree_);
}

std::string Configuration::to_string(ConfigurationGroup configuration_group) const {
    Dictionary::key_filter* key_filter = Dictionary::default_key_filter;
    switch(configuration_group) {
    case USER: {
        key_filter = [](std::string_view key) { return !key.starts_with('_'); };
        break;
    }
    case INTERNAL: {
        key_filter = [](std::string_view key) { return key.starts_with('_'); };
        break;
    }
    default: break;
    }
    return root_dictionary_.format(true, key_filter);
}

PayloadBuffer Configuration::assemble() const {
    msgpack::sbuffer sbuf {};
    ::msgpack_pack(sbuf, root_dictionary_);
    return {std::move(sbuf)};
}

Configuration Configuration::disassemble(const PayloadBuffer& message) {
    return {msgpack_unpack_to<Dictionary>(to_char_ptr(message.span().data()), message.span().size())};
}
