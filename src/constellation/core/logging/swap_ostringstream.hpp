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

namespace constellation::log {
    // Forward declaration of Logger class
    class Logger;

    // Class that swap content with Logger stream and calls Logger::flush
    class swap_ostringstream : public std::ostringstream {
    public:
        swap_ostringstream(Logger* logger) : logger_(logger) {}

        swap_ostringstream(swap_ostringstream const&) = delete;
        swap_ostringstream& operator=(swap_ostringstream const&) = delete;
        swap_ostringstream(swap_ostringstream&&) = delete;
        swap_ostringstream& operator=(swap_ostringstream&&) = delete;

        ~swap_ostringstream() override;

    private:
        Logger* logger_;
    };
} // namespace constellation::log
