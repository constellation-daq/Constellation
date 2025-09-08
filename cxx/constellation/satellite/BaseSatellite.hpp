/**
 * @file
 * @brief Base satellite with internal functionality
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/heartbeat/HeartbeatManager.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/CommandRegistry.hpp"
#include "constellation/satellite/FSM.hpp"

namespace constellation::satellite {

    class CNSTLN_API BaseSatellite {
    protected:
        BaseSatellite(std::string_view type, std::string_view name);

    public:
        /**
         * @brief Destruct base satellite
         *
         * @warning `BaseSatellite::join()` has to be called before destruction
         */
        virtual ~BaseSatellite() = default;

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        BaseSatellite(const BaseSatellite& other) = delete;
        BaseSatellite& operator=(const BaseSatellite& other) = delete;
        BaseSatellite(BaseSatellite&& other) = delete;
        BaseSatellite& operator=(BaseSatellite&& other) = delete;
        /// @endcond

    private:
        // FSM needs access to internal functions
        friend FSM;

        // Satellite needs access to internal variables
        friend class Satellite;

    public:
        /**
         * @brief Return the satellite type
         */
        constexpr std::string_view getSatelliteType() const { return satellite_type_; }

        /**
         * @brief Return the satellite name
         */
        constexpr std::string_view getSatelliteName() const { return satellite_name_; }

        /**
         * @brief Return the canonical name of the satellite
         *
         * The canonical name is satellite_type.satellite_name
         */
        std::string getCanonicalName() const;

        /**
         * @brief Returns if online configuration is supported
         */
        constexpr bool supportsReconfigure() const { return support_reconfigure_; }

        /**
         * @brief Return current state of the satellite
         */
        protocol::CSCP::State getState() const { return fsm_.getState(); }

        /**
         * @brief Return the current status of the satellite
         */
        std::string_view getStatus() const { return fsm_.getStatus(); }

        /**
         * @brief Return the current or last used run identifier
         */
        std::string_view getRunIdentifier() const { return run_identifier_; }

        /**
         * @brief Return the ephemeral port number to which the CSCP socket is bound to
         */
        constexpr networking::Port getCommandPort() const { return cscp_port_; }

        /**
         * @brief Return the ephemeral port number to which the CHP socket is bound to
         */
        constexpr networking::Port getHeartbeatPort() const { return heartbeat_manager_.getPort(); }

        /**
         * @brief Return the FSM of the satellite
         *
         * @warning Use carefully, the FSM gives direct access to low level functionality of the framework.
         */
        FSM& getFSM() { return fsm_; }

        /**
         * @brief Join CSCP processing thread
         *
         * Join the CSCP processing thread, which happens when the satellite is shut down or terminated.
         */
        void join();

        /**
         * @brief Terminate the satellite
         */
        void terminate();

        /**
         * @brief Check if the satellite has been terminated
         *
         * @return True if the satellite has been terminated
         */
        bool terminated() const { return terminated_.load(); }

    protected:
        /**
         * @brief Helper to check whether the current or past run has been marked as degraded
         * @return True if the run has been marked as degraded, false otherwise
         */
        bool is_run_degraded() const { return run_degraded_.load(); }

    private:
        /**
         * @brief Get the next CSCP command
         *
         * @return CSCP message if any received
         */
        std::optional<message::CSCP1Message> get_next_command();

        /**
         * @brief Send a reply for a CSCP command
         *
         * @param reply_verb CSCP reply verb
         * @param payload Optional message payload
         * @param tags Header metadata
         */
        void send_reply(std::pair<message::CSCP1Message::Type, std::string> reply_verb,
                        message::PayloadBuffer payload = {},
                        config::Dictionary tags = {});

        /**
         * @brief Handle standard CSCP commands
         *
         * @param command Command string to try to handle as standard command
         * @return CSCP reply verb and message payload if a standard command
         */
        std::optional<
            std::tuple<std::pair<message::CSCP1Message::Type, std::string>, message::PayloadBuffer, config::Dictionary>>
        handle_standard_command(std::string_view command);

        /**
         * @brief Handle user CSCP commands
         *
         * @param command Command string to try to handle as user command
         * @param payload CSCP message payload sent with command
         * @return CSCP reply verb and message payload if a standard command
         */
        std::optional<std::pair<std::pair<message::CSCP1Message::Type, std::string>, message::PayloadBuffer>>
        handle_user_command(std::string_view command, const message::PayloadBuffer& payload);

        /**
         * @brief CSCP loop handling incoming CSCP messages
         *
         * @param stop_token Stop token to interrupt the thread
         */
        void cscp_loop(const std::stop_token& stop_token);

        /**
         * @brief Parse and apply internal parameters for the satellite from the configuration
         */
        void apply_internal_config(const config::Configuration& config);

        /**
         * @brief Store configuration in satellite
         * @return Number of unused key-value pairs in the configuration
         */
        std::size_t store_config(config::Configuration&& config);

        /**
         * @brief Update configuration stored in satellite
         * @return Number of unused key-value pairs in the configuration
         */
        std::size_t update_config(const config::Configuration& partial_config);

        /**
         * @brief Helper to obtain the current user-defined status or return an alternative default message
         * @param message Default status message
         * @return Status message
         */
        std::string get_user_status_or(std::string message);

        /**
         * @brief Helper to set the user status
         * @param message User status message
         */
        void set_user_status(std::string message);

        /**
         * @brief Helper to mark a run as degraded
         */
        void mark_degraded(std::string_view reason);

    public:
        /// @cond doxygen_suppress
        virtual void initializing(config::Configuration& config) = 0;
        virtual void launching() = 0;
        virtual void landing() = 0;
        virtual void reconfiguring(const config::Configuration& partial_config) = 0;
        virtual void starting(std::string_view run_identifier) = 0;
        virtual void stopping() = 0;
        virtual void running(const std::stop_token& stop_token) = 0;
        virtual void interrupting(protocol::CSCP::State previous_state, std::string_view reason) = 0;
        virtual void failure(protocol::CSCP::State previous_state, std::string_view reason) = 0;
        /// @endcond

    private:
        std::optional<std::string> initializing_wrapper(config::Configuration&& config);
        std::optional<std::string> launching_wrapper();
        std::optional<std::string> landing_wrapper();
        std::optional<std::string> reconfiguring_wrapper(const config::Configuration& partial_config);
        std::optional<std::string> starting_wrapper(std::string run_identifier);
        std::optional<std::string> stopping_wrapper();
        std::optional<std::string> running_wrapper(const std::stop_token& stop_token);
        std::optional<std::string> interrupting_wrapper(protocol::CSCP::State previous_state, std::string_view reason);
        std::optional<std::string> failure_wrapper(protocol::CSCP::State previous_state, std::string_view reason);

    protected:
        log::Logger logger_; // NOLINT(misc-non-private-member-variables-in-classes)

    private:
        zmq::socket_t cscp_rep_socket_;
        networking::Port cscp_port_;

        std::string satellite_type_;
        std::string satellite_name_;
        FSM fsm_;

        std::jthread cscp_thread_;
        std::atomic_bool terminated_ {false};

        bool support_reconfigure_ {false};
        config::Configuration config_;
        std::string run_identifier_;
        std::atomic<bool> run_degraded_ {false};

        std::optional<std::string> user_status_;
        std::mutex user_status_mutex_;

        CommandRegistry user_commands_;
        heartbeat::HeartbeatManager heartbeat_manager_;
    };

} // namespace constellation::satellite
