/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <string_view>
#include <tuple>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

class DummyController : public constellation::controller::Controller {
public:
    DummyController(std::string controller_name) : Controller(controller_name) {}

    void reached_state(constellation::protocol::CSCP::State state, bool global) final {
        reached_state_global_ = global;
        reached_state_ = state;
    }

    void propagate_update(constellation::controller::Controller::UpdateType type,
                          std::size_t position,
                          std::size_t total) final {
        propagate_update_ = type;
        propagate_position_ = position;
        propagate_total_ = total;
    }

    std::tuple<constellation::protocol::CSCP::State, bool> last_reached_state() const {
        return {reached_state_.load(), reached_state_global_.load()};
    };

    std::tuple<constellation::controller::Controller::UpdateType, std::size_t, std::size_t> last_propagate_update() const {
        return {propagate_update_.load(), propagate_position_.load(), propagate_total_.load()};
    };

private:
    std::atomic_bool reached_state_global_ {true};
    std::atomic<constellation::protocol::CSCP::State> reached_state_ {constellation::protocol::CSCP::State::NEW};

    std::atomic_size_t propagate_total_ {0};
    std::atomic_size_t propagate_position_ {0};
    std::atomic<constellation::controller::Controller::UpdateType> propagate_update_ {};
};
