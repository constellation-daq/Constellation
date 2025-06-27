/**
 * @file
 * @brief Mattermost satellite
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

class MattermostSatellite final : public constellation::satellite::Satellite, constellation::listener::LogListener {
public:
    MattermostSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void starting(std::string_view run_identifier) final;
    void stopping() final;
    void interrupting(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;
    void failure(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;

private:
    enum class Priority : std::uint8_t {
        DEFAULT,
        STANDARD,
        IMPORTANT,
        URGENT,
    };
    using enum Priority;

private:
    void log_callback(constellation::message::CMDP1LogMessage msg);
    void send_message(std::string&& text,
                      Priority priority = DEFAULT,
                      std::string_view username = "",
                      std::string_view card = "");
    static std::string text_json(std::string&& text);
    static std::string priority_json(Priority priority);
    static std::string username_json(std::string_view username);
    static std::string card_json(std::string_view card);
    static std::string escape_quotes(std::string message);

private:
    std::string webhook_url_;
    constellation::utils::string_hash_set ignore_topics_;
    bool only_in_run_ {};
};
