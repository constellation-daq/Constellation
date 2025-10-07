/**
 * @file
 * @brief Implementation of manager locator
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ManagerLocator.hpp"

#include <memory>
#include <mutex>
#include <utility>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"

using namespace constellation;
using namespace constellation::log;
using namespace constellation::metrics;
using namespace constellation::utils;

ManagerLocator& ManagerLocator::getInstance() {
    static ManagerLocator instance {};
    return instance;
}

ManagerLocator::ManagerLocator() {
    // Creation order: global ZeroMQ context, SinkManager, MetricsManager, CHIRPManager
    zmq_context_ = networking::global_zmq_context(); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    sink_manager_ = std::unique_ptr<SinkManager>(new SinkManager());
    // Cannot create MetricsManager and CHIRP Manager during creation
    // since they require a ManagerLocator instance to get SinkManager instance for logging
}

ManagerLocator::~ManagerLocator() {
    // Stop the subscription loop in the CMDP sink
    sink_manager_->disableCMDPSending();
    // Destruction order: CHIRPManager, MetricsManager, SinkManager, global ZeroMQ context
    chirp_manager_.reset();
    metrics_manager_.reset();
    sink_manager_.reset();
    zmq_context_.reset();
}

void ManagerLocator::create_dependent_managers() {
    // Creation order: MetricsManager, CHIRPManager
    metrics_manager_ = std::unique_ptr<MetricsManager>(new MetricsManager());
    // TODO(stephan.lachnit): CHIRPManager requires rework to be able to be created here
}

SinkManager& ManagerLocator::getSinkManager() {
    return *getInstance().sink_manager_;
}

MetricsManager& ManagerLocator::getMetricsManager() {
    auto& instance = getInstance();
    std::call_once(instance.creation_flag_, [&]() { instance.create_dependent_managers(); });
    return *instance.metrics_manager_;
}

chirp::Manager* ManagerLocator::getCHIRPManager() {
    auto& instance = getInstance();
    // TODO(stephan.lachnit): use create_dependent_managers() once CHIRPManager has been reworked
    return instance.chirp_manager_.get();
}

void ManagerLocator::setDefaultCHIRPManager(std::unique_ptr<chirp::Manager> manager) {
    auto& instance = getInstance();
    instance.chirp_manager_ = std::move(manager);
}
