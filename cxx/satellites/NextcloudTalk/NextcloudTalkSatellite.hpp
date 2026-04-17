/**
 * @file
 * @brief Nextcloud Talk satellite
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

class NextcloudTalkSatellite final : public constellation::satellite::Satellite, constellation::listener::LogListener {
public:
    NextcloudTalkSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void starting(std::string_view run_identifier) final;
    void stopping() final;
    void interrupting(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;
    void failure(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;

private:
    void log_callback(constellation::message::CMDP1LogMessage msg);
    void send_message(const std::string& text);
    static std::string text_json(const std::string& text);
    static std::string escape_quotes(std::string message);

private:
    std::string url_;
    std::string auth_;
    constellation::utils::string_hash_set ignore_topics_;
    bool only_in_run_ {};
    std::chrono::milliseconds backoff_time_ {500};
    std::size_t max_retries_ {5};
};
