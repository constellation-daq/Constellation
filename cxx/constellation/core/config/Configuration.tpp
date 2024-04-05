/**
 * @file
 * @brief Template implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <algorithm>

#include <magic_enum.hpp>

#include "constellation/core/utils/string.hpp"

namespace constellation::config {
    /**
     * @throws MissingKeyError If the requested key is not defined
     * @throws InvalidTypeError If the conversion to the requested type did not succeed
     * @throws InvalidTypeError If an overflow happened while converting the key
     */
    template <typename T> T Configuration::get(const std::string& key) const {
        try {
            const auto dictval = config_.at(key);
            T val;
            // Value is directly held by variant:
            if constexpr(is_one_of<T, value_t>()) {
                val = std::get<T>(dictval);
            } else if constexpr(std::is_integral_v<T>) {
                val = static_cast<T>(std::get<std::int64_t>(dictval));
            } else if constexpr(std::is_floating_point_v<T>) {
                val = static_cast<T>(std::get<double>(dictval));
            } else if constexpr(std::is_enum_v<T>) {
                auto str = std::get<std::string>(dictval);
                std::transform(str.begin(), str.end(), str.begin(), ::toupper);
                auto enum_val = magic_enum::enum_cast<T>(str);

                if(!enum_val.has_value()) {
                    throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
                }

                val = enum_val.value();
            } else {
                throw std::bad_variant_access();
            }
            used_keys_.markUsed(key);
            return val;
        } catch(std::out_of_range& e) {
            throw MissingKeyError(key);
        } catch(std::bad_variant_access& e) {
            // Do not give an additional reason, the variant access is cryptic:
            throw InvalidTypeError(key, config_.at(key).type(), typeid(T));
        } catch(std::invalid_argument& e) {
            throw InvalidValueError(config_.at(key).str(), key, e.what());
        } catch(std::overflow_error& e) {
            throw InvalidTypeError(key, config_.at(key).type(), typeid(T), e.what());
        }
    }

    /**
     * @throws InvalidKeyError If the conversion to the requested type did not succeed
     * @throws InvalidKeyError If an overflow happened while converting the key
     */
    template <typename T> T Configuration::get(const std::string& key, const T& def) const {
        if(has(key)) {
            return get<T>(key);
        }
        return def;
    }

    /**
     * @throws MissingKeyError If the requested key is not defined
     * @throws InvalidKeyError If the conversion to the requested type did not succeed
     * @throws InvalidKeyError If an overflow happened while converting the key
     */
    template <typename T> std::vector<T> Configuration::getArray(const std::string& key) const {
        // Value is directly held by variant, let's return:
        if constexpr(is_one_of<std::vector<T>, value_t>()) {
            return get<std::vector<T>>(key);
        } else if constexpr(std::is_integral_v<T>) {
            auto vec = get<std::vector<std::int64_t>>(key);
            return std::vector<T>(vec.begin(), vec.end());
        } else if constexpr(std::is_floating_point_v<T>) {
            auto vec = get<std::vector<double>>(key);
            return std::vector<T>(vec.begin(), vec.end());
        } else if constexpr(std::is_enum_v<T>) {

            auto vec = get<std::vector<std::string>>(key);
            std::vector<T> result;
            result.reserve(vec.size());
            std::for_each(vec.begin(), vec.end(), [&](auto& str) {
                std::transform(str.begin(), str.end(), str.begin(), ::toupper);
                auto enum_val = magic_enum::enum_cast<T>(str);
                if(!enum_val.has_value()) {
                    throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
                }
                result.push_back(enum_val.value());
            });
            return result;
        } else {
            return get<std::vector<T>>(key);
        }
    }

    /**
     * @throws InvalidKeyError If the conversion to the requested type did not succeed
     * @throws InvalidKeyError If an overflow happened while converting the key
     */
    template <typename T> std::vector<T> Configuration::getArray(const std::string& key, const std::vector<T>& def) const {
        if(has(key)) {
            return getArray<T>(key);
        }
        return std::move(def);
    }

    template <typename T> void Configuration::set(const std::string& key, const T& val, bool mark_used) {
        if constexpr(is_one_of<T, value_t>()) {
            config_[key] = val;
        } else if constexpr(std::is_same_v<T, size_t>) {
            // FIXME check for overflow
            config_[key] = static_cast<std::int64_t>(val);
        } else if constexpr(std::is_integral_v<T>) {
            config_[key] = static_cast<std::int64_t>(val);
        } else if constexpr(std::is_floating_point_v<T>) {
            config_[key] = static_cast<double>(val);
        } else if constexpr(std::is_enum_v<T>) {
            config_[key] = std::string(magic_enum::enum_name<T>(val));
        } else {
            // FIXME throw something?
            config_[key] = val;
        }
        used_keys_.registerMarker(key);
        if(mark_used) {
            used_keys_.markUsed(key);
        }
    }

    template <typename T> void Configuration::setArray(const std::string& key, const std::vector<T>& val, bool mark_used) {
        if constexpr(is_one_of<std::vector<T>, value_t>()) {
            set<std::vector<T>>(key, val, mark_used);
        } else if constexpr(std::is_integral_v<T>) {
            // FIXME check for overflow
            std::vector<std::int64_t> nval(val.begin(), val.end());
            set(key, nval, mark_used);
        } else if constexpr(std::is_floating_point_v<T>) {
            std::vector<double> nval(val.begin(), val.end());
            set(key, nval, mark_used);
        } else if constexpr(std::is_enum_v<T>) {
            std::vector<std::string> nval;
            nval.reserve(val.size());

            std::for_each(val.begin(), val.end(), [&](auto& enum_val) {
                nval.emplace_back(magic_enum::enum_name<T>(enum_val));
            });
            set(key, nval, mark_used);
        } else {
            // FIXME throw something?
            set<std::vector<T>>(key, val, mark_used);
        }
    }

    template <typename T> void Configuration::setDefault(const std::string& key, const T& val) {
        if(!has(key)) {
            set<T>(key, val, true);
        }
    }

    template <typename T> void Configuration::setDefaultArray(const std::string& key, const std::vector<T>& val) {
        if(!has(key)) {
            setArray<T>(key, val, true);
        }
    }
} // namespace constellation::config
