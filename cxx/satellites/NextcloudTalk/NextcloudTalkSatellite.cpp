/**
 * @file
 * @brief Implementation of the Nextcloud Talk satellite
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "NextcloudTalkSatellite.hpp"

#include <chrono> // IWYU pragma: keep
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <thread>
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

#include "base64.hpp"

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

NextcloudTalkSatellite::NextcloudTalkSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("MNTR", [this](CMDP1Message&& msg) { log_callback(std::move(msg)); }) {}

void NextcloudTalkSatellite::initializing(Configuration& config) {

    url_ = config.get<std::string>("host") + "/ocs/v2.php/apps/spreed/api/v1/chat/" + config.get<std::string>("chat_id");
    auth_ = base64::to_base64(config.get<std::string>("account") + ":" + config.get<std::string>("app_password"));

    send_message(getCanonicalName() + " connected as logger");
    LOG(STATUS) << "Connected to Nextcloud Talk";

    const auto log_level = config.get<Level>("log_level", Level::WARNING);
    setGlobalLogLevel(log_level);
    LOG(STATUS) << "Set log level to " << log_level;

    const auto ignore_topics_v = config.getArray<std::string>("ignore_topics", {"FSM"});
    LOG_IF(INFO, !ignore_topics_v.empty()) << "Ignore log messages with topics " << range_to_string(ignore_topics_v);
    ignore_topics_.clear();
    ignore_topics_.insert(ignore_topics_v.begin(), ignore_topics_v.end());

    only_in_run_ = config.get<bool>("only_in_run", false);
    LOG_IF(INFO, only_in_run_) << "Only logging to Nextcloud Talk in RUN state";

    max_retries_ = config.get<std::size_t>("max_retries", 5);
    backoff_time_ = static_cast<std::chrono::milliseconds>(config.get<std::size_t>("backoff_time", 500));

    // Stop pool in case it was already started
    stopPool();
    startPool();
}

void NextcloudTalkSatellite::starting(std::string_view run_identifier) {
    send_message("@all Run " + quote(run_identifier) + " started");
}

void NextcloudTalkSatellite::stopping() {
    send_message("@all Run " + quote(getRunIdentifier()) + " stopped");
}

void NextcloudTalkSatellite::interrupting(State previous_state, std::string_view reason) {
    send_message("@all Interrupted: " + std::string(reason) + "\nPrevious state: " + std::string(enum_name(previous_state)));
}

void NextcloudTalkSatellite::failure(State /*previous_state*/, std::string_view /*reason*/) {
    stopPool();
}

void NextcloudTalkSatellite::log_callback(CMDP1LogMessage msg) {
    // Skip if only_in_run setting enabled but not in RUN or SAFE
    if(only_in_run_ && run_interrupting_safe.contains(getState())) {
        return;
    }
    // Skip if ignored topic
    if(ignore_topics_.contains(msg.getTopic())) {
        return;
    }
    // If warning or critical, prefix channel notification and set message priority
    std::string text {msg.getHeader().getSender()};
    text += " ";
    if(msg.getLogLevel() == WARNING || msg.getLogLevel() == CRITICAL) {
        text += "@all ";
    }

    // Add log message
    text += msg.getLogMessage();

    // Try to send message, on failure go to ERROR state
    try {
        send_message(text);
    } catch(const CommunicationError& error) {
        getFSM().requestFailure(error.what());
    }
}

void NextcloudTalkSatellite::send_message(const std::string& text) {

    // Try sending with exponential backoff:
    auto backoff = backoff_time_;
    const auto json_text = text_json(text);
    for(std::size_t attempt = 1; attempt <= max_retries_; ++attempt) {
        LOG(TRACE) << "Attempting to send text body:\n" << json_text;
        const auto response = cpr::Post(cpr::Url(url_),
                                        cpr::Header({{"Content-Type", "application/json"},
                                                     {"Accept", "application/json"},
                                                     {"OCS-APIRequest", "true"},
                                                     {"Authorization", "Basic " + auth_}}),
                                        cpr::Body({"{" + json_text + "}"}),
                                        cpr::Timeout({3s}));
        // Status code 201 indicates successful message creation
        if(response.status_code == 201) [[likely]] {
            return;
        }

        if(attempt == max_retries_) {
            throw CommunicationError(
                "Failed to send message to Nextcloud Talk: " +
                (response.error ? response.error.message : "Response " + std::to_string(response.status_code)));
        }

        LOG(DEBUG) << "Sending message failed, waiting for " << backoff << " before trying again";
        std::this_thread::sleep_for(backoff);
        backoff *= 2;
    }
}

std::string NextcloudTalkSatellite::text_json(const std::string& text) {
    constexpr const char* prefix = R"("message":")";
    constexpr const char* suffix = R"(")";
    return prefix + escape_quotes(text) + suffix;
}

std::string NextcloudTalkSatellite::escape_quotes(std::string message) {
    // Escape quotes to generate valid JSON
    std::string::size_type pos = 0;
    while((pos = message.find('"', pos)) != std::string::npos) {
        message.replace(pos, 1, "\\\"");
        pos += 2;
    }
    return message;
}
