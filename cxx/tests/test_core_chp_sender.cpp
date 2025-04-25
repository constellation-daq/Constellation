/**
 * @file
 * @brief Tests for CHP sender
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <future>
#include <string> // IWYU pragma: keep
#include <vector>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/protocol/CSCP_definitions.hpp"

#include "chp_mock.hpp"

using namespace constellation::protocol;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Send and receive an extrasystole", "[chp][send]") {
    // start chirp

    auto sender = CHPSender("dummy", std::chrono::milliseconds(10000));
    sender.setState(CSCP::State::NEW);
    sender.sendExtrasystole("test");
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
