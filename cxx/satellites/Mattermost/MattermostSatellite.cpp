/**
 * @file
 * @brief Implementation of the Mattermost satellite
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MattermostSatellite.hpp"

#include <chrono> // IWYU pragma: keep
#include <set>
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
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

namespace {
    // NOLINTNEXTLINE(cert-err58-cpp)
    const std::set<State> run_interrupting_safe {State::RUN, State::interrupting, State::SAFE};
} // namespace

MattermostSatellite::MattermostSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("MNTR", [this](CMDP1Message&& msg) { log_callback(std::move(msg)); }) {}

void MattermostSatellite::initializing(Configuration& config) {
    webhook_url_ = config.get<std::string>("webhook_url");
    send_message(getCanonicalName() + " connected as logger");
    LOG(STATUS) << "Connected to Mattermost";

    const auto log_level = config.get<Level>("log_level", Level::WARNING);
    setGlobalLogLevel(log_level);
    LOG(STATUS) << "Set log level to " << log_level;

    const auto ignore_topics_v = config.getArray<std::string>("ignore_topics", {"FSM"});
    LOG_IF(INFO, !ignore_topics_v.empty()) << "Ignore log messages with topics " << range_to_string(ignore_topics_v);
    ignore_topics_.clear();
    ignore_topics_.insert(ignore_topics_v.begin(), ignore_topics_v.end());

    only_in_run_ = config.get<bool>("only_in_run", false);
    LOG_IF(INFO, only_in_run_) << "Only logging to Mattermost in RUN state";

    // Stop pool in case it was already started
    stopPool();
    startPool();
}

void MattermostSatellite::starting(std::string_view run_identifier) {
    send_message("@channel Run " + quote(run_identifier) + " started");
}

void MattermostSatellite::stopping() {
    send_message("@channel Run " + quote(getRunIdentifier()) + " stopped");
}

void MattermostSatellite::interrupting(State previous_state, std::string_view reason) {
    send_message("@channel Interrupted: " + std::string(reason) +
                     "\nPrevious state: " + std::string(enum_name(previous_state)),
                 IMPORTANT);
}

void MattermostSatellite::failure(State /*previous_state*/, std::string_view /*reason*/) {
    stopPool();
}

void MattermostSatellite::log_callback(CMDP1LogMessage msg) {
    // Skip if only_in_run setting enabled but not in RUN or SAFE
    if(only_in_run_ && run_interrupting_safe.contains(getState())) {
        return;
    }
    // Skip if ignored topic
    if(ignore_topics_.contains(msg.getTopic())) {
        return;
    }
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

void MattermostSatellite::send_message(std::string&& text,
                                       Priority priority,
                                       std::string_view username,
                                       std::string_view card) {
    const auto response = cpr::Post(cpr::Url(webhook_url_),
                                    cpr::Header({{"Content-Type", "application/json"}}),
                                    cpr::Body({"{" + text_json(std::move(text)) + priority_json(priority) +
                                               username_json(username) + card_json(card) + "}"}),
                                    cpr::Timeout({2s}));
    if(response.error) [[unlikely]] {
        throw CommunicationError("Failed to send message to Mattermost: " + response.error.message);
    }
}

std::string MattermostSatellite::text_json(std::string&& text) {
    constexpr const char* prefix = R"("text":")";
    constexpr const char* suffix = R"(")";
    return prefix + escape_quotes(std::move(text)) + suffix;
}

std::string MattermostSatellite::priority_json(Priority priority) {
    constexpr const char* prefix = R"(,"priority":{"priority":")";
    constexpr const char* suffix = R"("})";
    switch(priority) {
    case STANDARD: return prefix + "standard"s + suffix;
    case IMPORTANT: return prefix + "important"s + suffix;
    case URGENT: return prefix + "urgent"s + suffix;
    default: return "";
    }
}

std::string MattermostSatellite::username_json(std::string_view username) {
    if(username.empty()) {
        return "";
    }
    constexpr const char* prefix = R"(,"username":")";
    constexpr const char* suffix = R"(")";
    return prefix + escape_quotes(std::string(username)) + suffix;
}

std::string MattermostSatellite::card_json(std::string_view card) {
    if(card.empty()) {
        return "";
    }
    constexpr const char* prefix = R"(,"props":{"card":")";
    constexpr const char* suffix = R"("})";
    return prefix + escape_quotes(std::string(card)) + suffix;
}

std::string MattermostSatellite::escape_quotes(std::string message) {
    // Escape quotes to generate valid JSON
    std::string::size_type pos = 0;
    while((pos = message.find('"', pos)) != std::string::npos) {
        message.replace(pos, 1, "\\\"");
        pos += 2;
    }
    return message;
}
