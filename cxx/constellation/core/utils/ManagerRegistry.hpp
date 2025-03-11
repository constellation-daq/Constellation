/**
 * @file
 * @brief Manager registry
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <mutex>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"

namespace constellation::utils {
    /**
     * @brief Manager registry
     *
     * This class is a singleton that manages the creation and destruction of various managers.
     */
    class ManagerRegistry {
    public:
        CNSTLN_API static ManagerRegistry& getInstance() {
            static ManagerRegistry instance {};
            return instance;
        }

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        ManagerRegistry(ManagerRegistry& other) = delete;
        ManagerRegistry& operator=(ManagerRegistry other) = delete;
        ManagerRegistry(ManagerRegistry&& other) = delete;
        ManagerRegistry& operator=(ManagerRegistry&& other) = delete;
        /// @endcond

        /**
         * @brief Return the sink manager
         */
        CNSTLN_API static log::SinkManager& getSinkManager() { return *getInstance().sink_manager_; }

        /**
         * @brief Return the metrics manager
         */
        CNSTLN_API static metrics::MetricsManager& getMetricsManager() {
            auto& instance = getInstance();
            std::call_once(instance.creation_flag_, [&]() { instance.create_dependent_managers(); });
            return *instance.metrics_manager_;
        }

        ~ManagerRegistry() {
            // Destruction order: MetricsManager, SinkManager, global ZeroMQ context
            metrics_manager_.reset();
            sink_manager_.reset();
            zmq_context_.reset();
        }

    private:
        ManagerRegistry() {
            // Creation order: global ZeroMQ context, SinkManager, MetricsManager
            zmq_context_ = networking::global_zmq_context(); // NOLINT(cppcoreguidelines-prefer-member-initializer)
            sink_manager_ = std::unique_ptr<log::SinkManager>(new log::SinkManager());
            // Cannot create MetricsManager duration creation: requires ManagerRegistry instance to get SinkManager instance
        }

        void create_dependent_managers() {
            metrics_manager_ = std::unique_ptr<metrics::MetricsManager>(new metrics::MetricsManager());
        }

    private:
        std::shared_ptr<zmq::context_t> zmq_context_;
        std::unique_ptr<metrics::MetricsManager> metrics_manager_;
        std::unique_ptr<log::SinkManager> sink_manager_;
        std::once_flag creation_flag_;
    };

} // namespace constellation::utils
