/**
 * @file
 * @brief Manager locator
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <mutex>
#include <utility>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"

namespace constellation::utils {
    /**
     * @brief Manager locator
     *
     * This class is a singleton that manages the access, creation and destruction of various managers.
     * It acts as a single global instance to avoid issues with static order initialization
     * when managers have dependencies on each other.
     */
    class ManagerLocator {
    public:
        CNSTLN_API static ManagerLocator& getInstance() {
            static ManagerLocator instance {};
            return instance;
        }

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        ManagerLocator(ManagerLocator& other) = delete;
        ManagerLocator& operator=(ManagerLocator other) = delete;
        ManagerLocator(ManagerLocator&& other) = delete;
        ManagerLocator& operator=(ManagerLocator&& other) = delete;
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

        /**
         * @brief Return the CHIRP manager
         */
        CNSTLN_API static chirp::Manager* getCHIRPManager() {
            auto& instance = getInstance();
            // TODO(stephan.lachnit): use create_dependent_managers() once CHIRPManager has been reworked
            return instance.chirp_manager_.get();
        }

        /**
         * @brief Create the default CHIRP manager
         */
        CNSTLN_API static void setDefaultCHIRPManager(std::unique_ptr<chirp::Manager> manager) {
            getInstance().chirp_manager_ = std::move(manager);
        }

        ~ManagerLocator() {
            // Stop the subscription loop in the CMDP sink
            sink_manager_->disableCMDPSending();
            // Destruction order: CHIRPManager, MetricsManager, SinkManager, global ZeroMQ context
            chirp_manager_.reset();
            metrics_manager_.reset();
            sink_manager_.reset();
            zmq_context_.reset();
        }

    private:
        ManagerLocator() {
            // Creation order: global ZeroMQ context, SinkManager, MetricsManager, CHIRPManager
            zmq_context_ = networking::global_zmq_context(); // NOLINT(cppcoreguidelines-prefer-member-initializer)
            sink_manager_ = std::unique_ptr<log::SinkManager>(new log::SinkManager());
            // Cannot create MetricsManager and CHIRP Manager during creation
            // since they require a ManagerLocator instance to get SinkManager instance for logging
        }

        void create_dependent_managers() {
            // Creation order: MetricsManager, CHIRPManager
            metrics_manager_ = std::unique_ptr<metrics::MetricsManager>(new metrics::MetricsManager());
            // TODO(stephan.lachnit): CHIRPManager requires rework to be able to be created here
        }

    private:
        std::shared_ptr<zmq::context_t> zmq_context_;
        std::unique_ptr<log::SinkManager> sink_manager_;
        std::unique_ptr<metrics::MetricsManager> metrics_manager_;
        std::unique_ptr<chirp::Manager> chirp_manager_;
        std::once_flag creation_flag_;
    };

} // namespace constellation::utils
