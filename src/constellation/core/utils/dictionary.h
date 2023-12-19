/**
 * @file
 * @brief Dictionary type
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace Constellation {
    using dictionary_t =
    std::map<std::string, std::variant<size_t, bool, int, double, std::string, std::chrono::system_clock::time_point>>;
}
