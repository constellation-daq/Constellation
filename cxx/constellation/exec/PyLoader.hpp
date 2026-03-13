/**
 * @file
 * @brief Loader for Python modules
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include "constellation/build.hpp"

namespace constellation::exec {

    class PyLoader {
    public:
        CNSTLN_API PyLoader();
        CNSTLN_API virtual ~PyLoader();

        // No copy/move constructor/assignment
        PyLoader(const PyLoader& other) = delete;
        PyLoader& operator=(const PyLoader& other) = delete;
        PyLoader(PyLoader&& other) = delete;
        PyLoader& operator=(PyLoader&& other) = delete;

        /**
         * @brief Load all satellite modules
         *
         * @return Map of satellite type names and corresponding Python modules
         */
        CNSTLN_API std::map<std::string, std::string> loadModules();

        /**
         * @brief Run the main from a Python module
         *
         * @param module Python module to load the main function from
         * @param args Arguments to the main function
         */
        CNSTLN_API void runModule(const std::string& module, const std::vector<std::string>& args);
    };

} // namespace constellation::exec
