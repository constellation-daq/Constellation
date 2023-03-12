// SPDX-FileCopyrightText: 2022-2023 Stephan Lachnit
// SPDX-License-Identifier: EUPL-1.2

#include "swap_ostringstream.hpp"

#include "Constellation/core/logging/Logger.hpp"

using namespace Constellation;

swap_ostringstream::~swap_ostringstream() {
    swap(logger_->os_);
    logger_->flush();
}
