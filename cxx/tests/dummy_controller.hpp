/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

#include "constellation/controller/Controller.hpp"
#include "constellation/controller/MeasurementCondition.hpp"
#include "constellation/controller/MeasurementQueue.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

class DummyController : public constellation::controller::Controller {
public:
    using Controller::UpdateType;

    DummyController(std::string controller_name) : Controller(std::move(controller_name)) {}

    void reached_state(constellation::protocol::CSCP::State state, bool global) final {
        reached_state_global_ = global;
        reached_state_ = state;
        reached_ = true;
    }

    void propagate_update(constellation::controller::Controller::UpdateType type,
                          std::size_t position,
                          std::size_t total) final {
        propagate_update_ = type;
        propagate_position_ = position;
        propagate_total_ = total;
        propagate_ = true;
    }

    std::tuple<UpdateType, std::size_t, std::size_t> lastPropagateUpdate() const {
        return {propagate_update_.load(), propagate_position_.load(), propagate_total_.load()};
    };

    void waitReachedState(constellation::protocol::CSCP::State state, bool global) {
        using namespace std::chrono_literals;

        // Wait for callback to trigger
        while(!reached_.load() || reached_state_.load() != state || reached_state_global_.load() != global) {
            std::this_thread::sleep_for(50ms);
        }
        reached_.store(false);
    }

    void waitPropagateUpdate() {
        using namespace std::chrono_literals;

        // Wait for callback to trigger
        while(!propagate_.load()) {
            std::this_thread::sleep_for(50ms);
        }
        propagate_.store(false);
    }

private:
    std::atomic_bool reached_ {false};
    std::atomic_bool reached_state_global_ {true};
    std::atomic<constellation::protocol::CSCP::State> reached_state_ {constellation::protocol::CSCP::State::NEW};

    std::atomic_bool propagate_ {false};
    std::atomic_size_t propagate_total_ {0};
    std::atomic_size_t propagate_position_ {0};
    std::atomic<constellation::controller::Controller::UpdateType> propagate_update_;
};

class DummyQueue : public constellation::controller::MeasurementQueue {
public:
    DummyQueue(DummyController& controller,
               std::string prefix,
               std::shared_ptr<constellation::controller::MeasurementCondition> condition,
               std::chrono::seconds timeout = std::chrono::seconds(1))
        : MeasurementQueue(controller, timeout) {
        setPrefix(std::move(prefix));
        setDefaultCondition(std::move(condition));
    }

    void queue_state_changed(constellation::controller::MeasurementQueue::State state, std::string_view reason) final {
        const std::lock_guard lock {mutex_};
        reason_ = reason;
        state_ = state;
        state_changed_ = true;
    };

    void progress_updated(std::size_t current, std::size_t total) final {
        progress_current_ = current;
        progress_total_ = total;
        progress_updated_ = true;
    }

    void waitStateChanged() {
        // Wait for callback to trigger
        while(!state_changed_.load()) {
            std::this_thread::yield();
        }
        state_changed_.store(false);
    }

    std::string getReason() {
        const std::lock_guard lock {mutex_};
        return reason_;
    }

    constellation::controller::MeasurementQueue::State getState() {
        const std::lock_guard lock {mutex_};
        return state_;
    }

    double waitProgress() {
        // Wait for callback to trigger
        while(!progress_updated_.load()) {
            std::this_thread::yield();
        }
        progress_updated_.store(false);
        return static_cast<double>(progress_current_.load()) / static_cast<double>(progress_total_.load());
    }

private:
    std::atomic_bool state_changed_ {false};
    std::atomic<constellation::controller::MeasurementQueue::State> state_;
    std::string reason_;
    std::mutex mutex_;

    std::atomic_bool progress_updated_ {false};
    std::atomic<std::size_t> progress_current_;
    std::atomic<std::size_t> progress_total_;
};
