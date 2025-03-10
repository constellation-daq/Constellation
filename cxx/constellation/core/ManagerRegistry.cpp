/**
 * @file
 * @brief Implementation of the manager registry
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ManagerRegistry.hpp"

#include <memory>

#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"

using namespace constellation::core;
using namespace constellation::log;
using namespace constellation::metrics;
using namespace constellation::networking;

ManagerRegistry& ManagerRegistry::getInstance() {
    static ManagerRegistry instance {};
    return instance;
}

ManagerRegistry::ManagerRegistry() {
    // Creation order: global ZeroMQ context, SinkManager, MetricsManager
    zmq_context_ = global_zmq_context(); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    sink_manager_ = std::shared_ptr<SinkManager>(new SinkManager());
    metrics_manager_ = std::shared_ptr<MetricsManager>(new MetricsManager());
}

ManagerRegistry::~ManagerRegistry() {
    // Destruction: MetricsManager, SinkManager, global ZeroMQ context
    metrics_manager_.reset();
    sink_manager_.reset();
    zmq_context_.reset();
}

SinkManager& ManagerRegistry::getSinkManager() {
    return *getInstance().sink_manager_;
}

MetricsManager& ManagerRegistry::getMetricsManager() {
    return *getInstance().metrics_manager_;
}
