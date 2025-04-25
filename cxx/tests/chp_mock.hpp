/**
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

#include "chirp_mock.hpp"

// CHPSender with
// state callback to internal state variable
// send_extrasystole(state, status) that sets state and then calls sendExtrasystole
// FIXME how to mock its CHIRP service, need to set manager before it starts pool/chirp manager

class CHPSender : public constellation::heartbeat::HeartbeatSend {
public:
    CHPSender(std::string name, std::chrono::milliseconds interval)
        : HeartbeatSend(std::move(name), [&]() { return getState(); }, interval) {}

    constellation::protocol::CSCP::State getState() const { return state_; }

    void setState(constellation::protocol::CSCP::State state) { state_ = state; }

private:
    constellation::protocol::CSCP::State state_;
};
