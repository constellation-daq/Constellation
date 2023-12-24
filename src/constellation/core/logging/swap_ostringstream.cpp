/**
 * @file
 * @brief Implementation of stream swap helper
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "swap_ostringstream.hpp"

#include "constellation/core/logging/Logger.hpp"

using namespace constellation;

swap_ostringstream::~swap_ostringstream() {
    swap(logger_->os_);
    logger_->flush();
}
