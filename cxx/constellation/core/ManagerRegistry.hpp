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

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"

namespace constellation::core {

    class ManagerRegistry {
    public:
        CNSTLN_API static ManagerRegistry& getInstance();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        ManagerRegistry(ManagerRegistry& other) = delete;
        ManagerRegistry& operator=(ManagerRegistry other) = delete;
        ManagerRegistry(ManagerRegistry&& other) = delete;
        ManagerRegistry& operator=(ManagerRegistry&& other) = delete;
        /// @endcond

        ~ManagerRegistry();

        /**
         * @brief Return the sink manager
         */
        CNSTLN_API static log::SinkManager& getSinkManager();

        /**
         * @brief Return the metrics manager
         */
        CNSTLN_API static metrics::MetricsManager& getMetricsManager();

    private:
        ManagerRegistry();

    private:
        std::shared_ptr<zmq::context_t> zmq_context_;
        std::shared_ptr<metrics::MetricsManager> metrics_manager_;
        std::shared_ptr<log::SinkManager> sink_manager_;
    };

} // namespace constellation::core
