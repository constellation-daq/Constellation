/**
 * @file
 * @brief Template implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

namespace constellation::config {
    /**
     * @throws MissingKeyError If the requested key is not defined
     * @throws InvalidKeyError If the conversion to the requested type did not succeed
     * @throws InvalidKeyError If an overflow happened while converting the key
     */
    template <typename T> T Configuration::get(const std::string& key) const {
        try {
            const auto dictval = config_.at(key);
            const auto val = std::get<T>(dictval);
            used_keys_.markUsed(key);
            return val;
        } catch(std::out_of_range& e) {
            throw MissingKeyError(key);
        } catch(std::bad_variant_access& e) {
            throw InvalidKeyError(key, config_.at(key).type(), typeid(T), e.what());
        } catch(std::invalid_argument& e) {
            throw InvalidKeyError(key, config_.at(key).type(), typeid(T), e.what());
        } catch(std::overflow_error& e) {
            throw InvalidKeyError(key, config_.at(key).type(), typeid(T), e.what());
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
        return get<std::vector<T>>(key);
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
        // FIXME need to make sure *can* actually set this and throw error otherwise
        config_[key] = val;
        used_keys_.registerMarker(key);
        if(mark_used) {
            used_keys_.markUsed(key);
        }
    }

    template <typename T> void Configuration::setArray(const std::string& key, const std::vector<T>& val, bool mark_used) {
        set<std::vector<T>>(key, val, mark_used);
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
