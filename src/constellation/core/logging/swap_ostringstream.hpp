/**
 * @file
 * @brief Stream swap helper
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <sstream>

namespace constellation {
    // Forward declaration of Logger class
    class Logger;

    // Class that swap content with Logger stream and calls Logger::flush
    class swap_ostringstream : public std::ostringstream {
    public:
        swap_ostringstream(Logger* logger) : logger_(logger) {}

        virtual ~swap_ostringstream();

    private:
        Logger* logger_;
    };
} // namespace constellation
