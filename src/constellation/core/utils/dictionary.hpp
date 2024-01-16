/**
 * @file
 * @brief Dictionary type for msgpack
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <map>
#include <string>

namespace constellation {
    using dictionary_t =
        std::map<std::string, std::variant<bool, std::int64_t, double, std::string, std::chrono::system_clock::time_point>>;
} // namespace constellation
