/**
 * @file
 * @brief Qt resource initialization code
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "constellation/gui/qt_utils.hpp"

// NOLINTNEXTLINE(misc-use-anonymous-namespace)
static void CNSTLN_INIT_RESOURCE() {
    Q_INIT_RESOURCE(Constellation);
}

void constellation::gui::initResources() {
    ::CNSTLN_INIT_RESOURCE();
}
