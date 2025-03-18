/**
 * @file
 * @brief Implementation of the MattermostLogger satellite
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MattermostLoggerSatellite.hpp"

#include <chrono> // IWYU pragma: keep
#include <string>
#include <string_view>
#include <utility>

#include <cpr/cpr.h>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

MattermostLoggerSatellite::MattermostLoggerSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("MATTERMOST", [this](CMDP1Message&& msg) { log_callback(std::move(msg)); }) {}

void MattermostLoggerSatellite::initializing(Configuration& config) {
    webhook_url_ = config.get<std::string>("webhook_url");
    send_message(getCanonicalName() + " connected as logger");
    LOG(STATUS) << "Connected to Mattermost";

    const auto log_level = config.get<Level>("log_level", Level::WARNING);
    setGlobalLogLevel(log_level);
    LOG(STATUS) << "Set log level to " << log_level;

    // Stop pool in case it was already started
    stopPool();
    startPool();
}

void MattermostLoggerSatellite::starting(std::string_view run_identifier) {
    send_message("@channel Run " + std::string(run_identifier) + " started");
}

void MattermostLoggerSatellite::stopping() {
    send_message("@channel Run " + std::string(getRunIdentifier()) + " stopped");
}

void MattermostLoggerSatellite::interrupting(CSCP::State previous_state) {
    send_message("@channel Interrupted! Previous state: " + std::string(enum_name(previous_state)), IMPORTANT);
}

void MattermostLoggerSatellite::failure(CSCP::State /*previous_state*/) {
    stopPool();
}

void MattermostLoggerSatellite::log_callback(CMDP1LogMessage msg) {
    // If warning or critical, prefix channel notification and set message priority
    std::string text {};
    Priority priority {DEFAULT};
    if(msg.getLogLevel() == WARNING) {
        text = "@channel ";
        priority = IMPORTANT;
    } else if(msg.getLogLevel() == CRITICAL) {
        text = "@channel ";
        priority = URGENT;
    }
    // Add log message
    text += msg.getLogMessage();
    // Add level and topic to card
    auto card = "**Level**: " + enum_name(msg.getLogLevel()) + "\\n\\n**Topic**: ";
    card += msg.getLogTopic();
    // Try to send message, on failure go to ERROR state
    try {
        send_message(std::move(text), priority, msg.getHeader().getSender(), std::move(card));
    } catch(const CommunicationError& error) {
        getFSM().requestFailure(error.what());
    }
}

void MattermostLoggerSatellite::send_message(std::string&& text,
                                             Priority priority,
                                             std::string_view username,
                                             std::string_view card) {
    const auto response = cpr::Post(cpr::Url(webhook_url_),
                                    cpr::Header({{"Content-Type", "application/json"}}),
                                    cpr::Body({"{" + text_json(std::move(text)) + priority_json(priority) +
                                               username_json(username) + card_json(card) + "}"}),
                                    cpr::Timeout({1s}));
    if(response.error) [[unlikely]] {
        throw CommunicationError("Failed to send message to Mattermost: " + response.error.message);
    }
}

std::string MattermostLoggerSatellite::text_json(std::string&& text) {
    constexpr const char* prefix = R"("text":")";
    constexpr const char* suffix = R"(")";
    return prefix + escape_quotes(std::move(text)) + suffix;
}

std::string MattermostLoggerSatellite::priority_json(Priority priority) {
    constexpr const char* prefix = R"(,"priority":{"priority":")";
    constexpr const char* suffix = R"("})";
    switch(priority) {
    case STANDARD: return prefix + "standard"s + suffix;
    case IMPORTANT: return prefix + "important"s + suffix;
    case URGENT: return prefix + "urgent"s + suffix;
    default: return "";
    }
}

std::string MattermostLoggerSatellite::username_json(std::string_view username) {
    if(username.empty()) {
        return "";
    }
    constexpr const char* prefix = R"(,"username":")";
    constexpr const char* suffix = R"(")";
    return prefix + escape_quotes(std::string(username)) + suffix;
}

std::string MattermostLoggerSatellite::card_json(std::string_view card) {
    if(card.empty()) {
        return "";
    }
    constexpr const char* prefix = R"(,"props":{"card":")";
    constexpr const char* suffix = R"("})";
    return prefix + escape_quotes(std::string(card)) + suffix;
}

std::string MattermostLoggerSatellite::escape_quotes(std::string message) {
    // Escape quotes to generate valid JSON
    std::string::size_type pos = 0;
    while((pos = message.find('"', pos)) != std::string::npos) {
        message.replace(pos, 1, "\\\"");
        pos += 2;
    }
    return message;
}
