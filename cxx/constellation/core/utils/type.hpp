/**
 * @file
 * @brief Tags for type dispatching and run time type identification
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#include <string>

namespace constellation::utils {
    /**
     * @brief Tag for specific type
     */
    template <typename T> struct type_tag {};
    /**
     * @brief Empty tag
     */
    struct empty_tag {};

#ifdef __GNUG__
    // Only demangled for GNU compiler
    inline std::string demangle(const char* name, bool keep_prefix = false) {
        // Try to demangle
        int status = -1;
        const std::unique_ptr<char, void (*)(void*)> res {abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};

        if(status == 0) {
            // Remove allpix tag if necessary
            std::string str = res.get();
            if(!keep_prefix && str.find("constellation::") == 0) {
                return str.substr(8);
            }
            return str;
        }
        return name;
    }

#else
    /**
     * @brief Demangle the type to human-readable form if it is mangled
     * @param name The possibly mangled name
     * @param keep_prefix If true the allpix namespace tag will be kept, otherwise it is removed
     */
    inline std::string demangle(const char* name, bool keep_prefix = false) {
        return name;
    }
#endif
} // namespace constellation::utils
