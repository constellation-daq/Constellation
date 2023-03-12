// SPDX-FileCopyrightText: 2022-2023 Stephan Lachnit
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <string>

namespace Constellation {
    namespace Satellite {
        // Starts a Satellite and loops until quitted
        void StartSatellite(std::string constellation_name);
    }
}
