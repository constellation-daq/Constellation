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

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"

namespace constellation::utils {
    /**
     * @brief Manager locator
     *
     * This class is a singleton that manages the access, creation and destruction of various managers.
     * It acts as a single global instance to avoid issues with static order initialization
     * when managers have dependencies on each other.
     */
    class CNSTLN_API ManagerLocator {
    public:
        static ManagerLocator& getInstance();

        ~ManagerLocator();

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
        static log::SinkManager& getSinkManager();

        /**
         * @brief Return the metrics manager
         */
        static metrics::MetricsManager& getMetricsManager();

        /**
         * @brief Return the CHIRP manager
         */
        static chirp::Manager* getCHIRPManager();

        /**
         * @brief Create the default CHIRP manager
         */
        static void setDefaultCHIRPManager(std::unique_ptr<chirp::Manager> manager);

    private:
        CNSTLN_LOCAL ManagerLocator();
        CNSTLN_LOCAL void create_dependent_managers();

    private:
        std::shared_ptr<zmq::context_t> zmq_context_;
        std::unique_ptr<log::SinkManager> sink_manager_;
        std::unique_ptr<metrics::MetricsManager> metrics_manager_;
        std::unique_ptr<chirp::Manager> chirp_manager_;
        std::once_flag creation_flag_;
    };

} // namespace constellation::utils
