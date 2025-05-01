/**
 * @file
 * @brief Helper class for queuing measurement series
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <map>
#include <queue>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"

namespace constellation::controller {

    /** Measurement queue class which allows to queue and fetch measurement configurations which can be used to reconfigure
     * a constellation
     */
    class CNSTLN_API MeasurementQueue {
    public:
        /** Measurement is a map with satellite canonical names as keys and configuration dictionaries as values */
        using Measurement = std::map<std::string, Controller::CommandPayload>;

        /**
         * @brief Construct a measurement queue
         *
         * @param controller Reference to the controller object to be used
         */
        MeasurementQueue(Controller& controller) : logger_("QUEUE"), controller_(controller) {};

        /**
         * @brief Destruct the measurement queue
         */
        virtual ~MeasurementQueue();

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        MeasurementQueue(const MeasurementQueue& other) = delete;
        MeasurementQueue& operator=(const MeasurementQueue& other) = delete;
        MeasurementQueue(MeasurementQueue&& other) = delete;
        MeasurementQueue& operator=(MeasurementQueue&& other) = delete;
        /// @endcond

        void append(Measurement measurement);

        bool running() const { return queue_thread_.joinable(); }

        std::size_t size() { return measurements_.size(); }

        /**
         * @brief Current progress of the measurement queue
         * @details Returns the fraction of measurements successfully performed over the total number of measurements. Since
         * measurements are popped from the queue once they have been done, this uses a separate counter for the total
         * number of measurements to be performed.
         *
         * @return Progress of the queue
         */
        double progress() { return static_cast<double>(size_at_start_ - measurements_.size()) / size_at_start_; }

        /**
         * @brief Start the measurement queue
         *
         * @note Requires the constellation to be in global state ORBIT
         */
        void start();

        /**
         * @brief Halt the measurement queue after the current measurement has concluded
         */
        void halt();

        /**
         * @brief Interrupt the current measurement and halt the queue
         */
        void interrupt();

    private:
        void queue_loop(const std::stop_token& stop_token);

        void await_state(protocol::CSCP::State state) const;

        void check_replies(const std::map<std::string, message::CSCP1Message>& replies) const;

    private:
        /** Logger to use */
        log::Logger logger_;

        std::string run_identifier_prefix_ {"queue_run_"};
        std::chrono::seconds transition_timeout_ {60};

        /** Queue of measurements */
        std::queue<Measurement> measurements_;

        /** Number of queue elements at the start of the measurements */
        std::size_t size_at_start_;

        /** Controller to use for the measurements */
        Controller& controller_;

        /** Queue thread */
        std::jthread queue_thread_;
        std::atomic_bool queue_running_ {false};
    };

} // namespace constellation::controller
