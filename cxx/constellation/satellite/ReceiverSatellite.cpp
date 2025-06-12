/**
 * @file
 * @brief Implementation of a data receiving satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ReceiverSatellite.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <mutex>
#include <optional>
#include <ranges>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::networking;
using namespace constellation::pools;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

ReceiverSatellite::ReceiverSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name),
      BasePool("CDTP", [this](CDTP1Message&& message) { this->handle_cdtp_message(std::move(message)); }),
      cdtp_logger_("CDTP") {

    register_metric("OUTPUT_FILE", "", MetricType::LAST_VALUE, "Current output file path. Updated when changed.");

    register_timed_metric("BYTES_RECEIVED",
                          "B",
                          MetricType::LAST_VALUE,
                          "Number of bytes received by this satellite in the current run",
                          10s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return bytes_received_.load(); });
}

void ReceiverSatellite::validate_output_directory(const std::filesystem::path& path) {
    try {
        // Create all the required directories
        std::filesystem::create_directories(path);

        // Convert the file to an absolute path
        const auto dir = std::filesystem::canonical(path);

        // Check that output directory is a directory indeed
        if(!std::filesystem::is_directory(dir)) {
            throw SatelliteError("Requested output directory " + dir.string() + " is not a directory");
        }

        // Register or update disk space metrics:
        register_diskspace_metric(dir);

    } catch(std::filesystem::filesystem_error& e) {
        const auto msg = std::string("Issue with output directory: ") + e.what();
        throw SatelliteError(msg);
    }
}

std::filesystem::path ReceiverSatellite::validate_output_file(const std::filesystem::path& path,
                                                              const std::string& file_name,
                                                              const std::string& ext) {
    // Create full file path from directory and name
    std::filesystem::path file = path / file_name;

    try {
        // Replace extension if desired
        if(!ext.empty()) {
            file.replace_extension(ext);
        }

        // Create all the required main directories and possible sub-directories from the filename
        std::filesystem::create_directories(file.parent_path());

        // Check if file exists
        if(std::filesystem::is_regular_file(file)) {
            if(!allow_overwriting_) {
                throw SatelliteError("Overwriting of existing file " + file.string() + " denied");
            }
            LOG(cdtp_logger_, WARNING) << "File " << file << " exists and will be overwritten";
            std::filesystem::remove(file);
        } else if(std::filesystem::is_directory(file)) {
            throw SatelliteError("Requested output file " + file.string() + " is an existing directory");
        }

        // Open the file to check if it can be accessed
        const auto file_stream = std::ofstream(file);
        if(!file_stream.good()) {
            throw SatelliteError("File " + file.string() + " not accessible");
        }

        // Convert the file to an absolute path
        file = std::filesystem::canonical(file);

        // Register or update disk space metrics:
        register_diskspace_metric(file);

    } catch(std::filesystem::filesystem_error& e) {
        const auto msg = std::string("Issue with output path: ") + e.what();
        throw SatelliteError(msg);
    }

    // Send metric with new output file path
    STAT("OUTPUT_FILE", file.string());

    return file;
}

std::ofstream ReceiverSatellite::create_output_file(const std::filesystem::path& path,
                                                    const std::string& file_name,
                                                    const std::string& ext,
                                                    bool binary) {
    // Validate and build absolute path:
    const auto file = validate_output_file(path, file_name, ext);

    // Open file stream and return
    auto stream = std::ofstream(file, binary ? std::ios_base::out | std::ios_base::binary : std::ios_base::out);
    return stream;
}

void ReceiverSatellite::register_diskspace_metric(const std::filesystem::path& path) {

    register_timed_metric("DISKSPACE_FREE",
                          "MiB",
                          MetricType::LAST_VALUE,
                          "Available disk space at the target location of the output file",
                          10s,
                          [this, path]() -> std::optional<uint64_t> {
                              try {
                                  const auto space = std::filesystem::space(path);
                                  LOG(cdtp_logger_, TRACE) << "Disk space capacity:  " << space.capacity;
                                  LOG(cdtp_logger_, TRACE) << "Disk space free:      " << space.free;
                                  LOG(cdtp_logger_, TRACE) << "Disk space available: " << space.available;

                                  const auto available_mb = space.available >> 20U;

                                  // Less than 10GiB disk space - let's warn the user via logs!
                                  if(available_mb >> 10U < 3) {
                                      LOG(cdtp_logger_, CRITICAL)
                                          << "Available disk space critically low, " << available_mb << "MiB left";
                                  } else if(available_mb >> 10U < 10) {
                                      LOG(cdtp_logger_, WARNING)
                                          << "Available disk space low, " << available_mb << "MiB left";
                                  }

                                  return {available_mb};
                              } catch(const std::filesystem::filesystem_error& e) {
                                  LOG(cdtp_logger_, WARNING) << e.what();
                              }
                              return std::nullopt;
                          });
}

void ReceiverSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        // Check and rethrow exception from BasePool
        checkPoolException();

        // Wait a bit to avoid hot loop
        std::this_thread::sleep_for(100ms);
    }
}

bool ReceiverSatellite::should_connect(const chirp::DiscoveredService& service) {
    if(!data_transmitters_.empty()) {
        return std::ranges::any_of(data_transmitters_,
                                   [=](const auto& data_tramsitter) { return service.host_id == MD5Hash(data_tramsitter); });
    }
    // If not set accept all incoming connections
    return true;
}

void ReceiverSatellite::initializing_receiver(Configuration& config) {
    allow_overwriting_ = config.get<bool>("_allow_overwriting", false);
    LOG(cdtp_logger_, DEBUG) << (allow_overwriting_ ? "Not allowing" : "Allowing") << " overwriting of files";

    data_transmitters_ = config.getSet<std::string>("_data_transmitters", {});
    if(data_transmitters_.empty()) {
        LOG(cdtp_logger_, INFO) << "Initialized to receive data from all transmitters";
    } else {
        std::ranges::for_each(data_transmitters_, [&](const auto& sat) {
            if(!CSCP::is_valid_canonical_name(sat)) {
                throw InvalidValueError(config, "_data_transmitters", "`" + sat + "` is not a valid canonical name");
            }
        });
        LOG(cdtp_logger_, INFO) << "Initialized to receive data from " << range_to_string(data_transmitters_);
    }
    reset_data_transmitter_states();

    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_eor_timeout", 10));
    LOG(cdtp_logger_, DEBUG) << "Timeout for EOR message " << data_eor_timeout_;
}

void ReceiverSatellite::reconfiguring_receiver(const Configuration& partial_config) {
    if(partial_config.has("_allow_overwriting")) {
        allow_overwriting_ = partial_config.get<bool>("_allow_overwriting");
        LOG(cdtp_logger_, DEBUG) << "Reconfigured to " << (allow_overwriting_ ? "not " : "") << "allow overwriting of files";
    }

    if(partial_config.has("_data_transmitters")) {
        throw InvalidKeyError("_data_transmitters", "Reconfiguration of data transmitters not possible");
    }

    if(partial_config.has("_eor_timeout")) {
        data_eor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_eor_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for EOR message: " << data_eor_timeout_;
    }
}

void ReceiverSatellite::starting_receiver() {
    // Reset all transmitters to not connected
    reset_data_transmitter_states();

    // Reset bytes received metric
    bytes_received_ = 0;
    STAT("BYTES_RECEIVED", 0);

    // Start BasePool thread
    startPool();
}

void ReceiverSatellite::stopping_receiver() {
    // Wait until no more events returned by poller
    while(pollerEvents() > 0) {
        // Check any exceptions that prevents poller from continuing
        checkPoolException();

        LOG(cdtp_logger_, TRACE) << "Poller still returned events, waiting before checking for EOR arrivals";

        // Wait a bit to avoid hot loop
        std::this_thread::sleep_for(100ms);
    }
    // Now start EOR timer
    LOG(cdtp_logger_, DEBUG) << "Starting timeout for EOR arrivals (" << data_eor_timeout_ << ")";
    TimeoutTimer timer {data_eor_timeout_};
    timer.reset();

    // Warn about transmitters that never sent a BOR message
    if(cdtp_logger_.shouldLog(WARNING)) {
        const std::lock_guard data_transmitter_states_lock {data_transmitter_states_mutex_};
        auto data_transmitters_not_connected =
            std::ranges::views::filter(data_transmitter_states_, [](const auto& data_transmitter_p) {
                return data_transmitter_p.second.state == TransmitterState::NOT_CONNECTED;
            });
        // Note: we do not have a biderectional range so the content needs to be copied to a vector
        LOG_IF(cdtp_logger_, WARNING, !data_transmitters_not_connected.empty())
            << "BOR message never send from "
            << range_to_string(std::ranges::to<std::vector>(std::views::keys(data_transmitters_not_connected)));

        // Warn about missed messages (so far)
        auto data_transmitters_missed_msg = std::ranges::views::filter(
            data_transmitter_states_, [](const auto& data_transmitter_p) { return data_transmitter_p.second.missed > 0; });
        LOG_IF(cdtp_logger_, WARNING, !data_transmitters_missed_msg.empty())
            << "Missed messages from "
            << range_to_string(std::ranges::to<std::vector>(std::views::keys(data_transmitters_missed_msg)))
            << ", data might be incomplete";
    }

    // Loop until all data transmitters that sent a BOR also sent an EOR
    while(true) {
        std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
        const auto missing_eors = std::ranges::count_if(data_transmitter_states_, [](const auto& data_transmitter_p) {
            return data_transmitter_p.second.state == TransmitterState::BOR_RECEIVED;
        });
        data_transmitter_states_lock.unlock();

        // Check if EOR messages are missing
        if(missing_eors == 0) {
            break;
        }

        // If timeout reached, throw
        if(timer.timeoutReached()) {
            // Stop BasePool thread and disconnect all connected sockets
            stopPool();

            // Filter for data transmitters that did not send an EOR
            auto data_transmitter_no_eor =
                std::ranges::views::filter(data_transmitter_states_, [](const auto& data_transmitter_p) {
                    return data_transmitter_p.second.state == TransmitterState::BOR_RECEIVED;
                });

            // Note: we do not have a bidirectional range so the content needs to be copied to a vector
            const auto data_transmitter_no_eor_str =
                range_to_string(std::ranges::to<std::vector>(std::ranges::views::keys(data_transmitter_no_eor)));
            LOG(cdtp_logger_, WARNING) << "Not all EOR messages received, emitting substitute EOR messages for "
                                       << data_transmitter_no_eor_str;

            for(const auto& data_transmitter : data_transmitter_no_eor) {
                LOG(cdtp_logger_, DEBUG) << "Creating substitute EOR for " << data_transmitter.first;
                auto run_metadata = Dictionary();
                run_metadata["run_id"] = getRunIdentifier();
                auto condition_code = CDTP::RunCondition::ABORTED;
                if(data_transmitter.second.missed > 0) {
                    condition_code |= CDTP::RunCondition::INCOMPLETE;
                }
                if(is_run_degraded()) {
                    condition_code |= CDTP::RunCondition::DEGRADED;
                }
                run_metadata["condition_code"] = condition_code;
                run_metadata["condition"] = enum_name(condition_code);
                receive_eor({data_transmitter.first, 0, CDTP1Message::Type::EOR}, std::move(run_metadata));
            }

            throw RecvTimeoutError("EOR messages missing from " + data_transmitter_no_eor_str, data_eor_timeout_);
        }

        // Wait a bit before re-locking
        std::this_thread::sleep_for(50ms);
    }

    LOG(cdtp_logger_, DEBUG) << "All EOR messages received";

    // Stop BasePool thread and disconnect all connected sockets
    stopPool();
}

void ReceiverSatellite::interrupting_receiver(CSCP::State previous_state) {
    // If in RUN stop as usual but do not throw if not all EOR messages received
    if(previous_state == CSCP::State::RUN) {
        try {
            stopping_receiver();
        } catch(const RecvTimeoutError& e) {
            LOG(cdtp_logger_, WARNING) << e.what();
        }
    }
}

void ReceiverSatellite::failure_receiver() {
    // Stop BasePool thread and disconnect all connected sockets
    stopPool();
}

void ReceiverSatellite::reset_data_transmitter_states() {
    const std::lock_guard data_transmitter_states_lock {data_transmitter_states_mutex_};
    data_transmitter_states_.clear();
    for(const auto& data_transmitter : data_transmitters_) {
        data_transmitter_states_.emplace(data_transmitter, TransmitterStateSeq(TransmitterState::NOT_CONNECTED, 0, 0));
    }
}

void ReceiverSatellite::handle_cdtp_message(CDTP1Message&& message) {
    using enum CDTP1Message::Type;
    switch(message.getHeader().getType()) {
    case BOR: {
        LOG(cdtp_logger_, DEBUG) << "Received BOR message from " << message.getHeader().getSender();
        handle_bor_message(std::move(message));
        break;
    }
    [[likely]] case DATA: {
        LOG(cdtp_logger_, TRACE) << "Received data message " << message.getHeader().getSequenceNumber() << " from "
                                 << message.getHeader().getSender();
        bytes_received_ += message.countPayloadBytes();
        handle_data_message(std::move(message));
        break;
    }
    case EOR: {
        LOG(cdtp_logger_, DEBUG) << "Received EOR message from " << message.getHeader().getSender();
        handle_eor_message(std::move(message));
        break;
    }
    default: std::unreachable();
    }
}

void ReceiverSatellite::handle_bor_message(CDTP1Message bor_message) {
    const auto sender = bor_message.getHeader().getSender();
    std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
    auto data_transmitter_it = data_transmitter_states_.find(sender);
    // Check that transmitter is not connected yet
    if(data_transmitter_it != data_transmitter_states_.cend()) {
        // If stored states, check that not connected yet
        if(data_transmitter_it->second.state != TransmitterState::NOT_CONNECTED) [[unlikely]] {
            throw InvalidCDTPMessageType(CDTP1Message::Type::BOR, "already received BOR " + std::string(sender));
        }
        data_transmitter_it->second.state = TransmitterState::BOR_RECEIVED;
    } else {
        // Otherwise, emplace
        data_transmitter_states_.emplace(sender, TransmitterState::BOR_RECEIVED);
    }
    data_transmitter_states_lock.unlock();

    LOG(cdtp_logger_, INFO) << "Received BOR from " << sender;
    receive_bor(bor_message.getHeader(), {Dictionary::disassemble(bor_message.getPayload().at(0)), true});
}

void ReceiverSatellite::handle_data_message(CDTP1Message data_message) {
    const auto sender = data_message.getHeader().getSender();
    std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
    const auto data_transmitter_it = data_transmitter_states_.find(sender);
    // Check that BOR was received
    if(data_transmitter_it == data_transmitter_states_.cend() ||
       data_transmitter_it->second.state != TransmitterState::BOR_RECEIVED) [[unlikely]] {
        throw InvalidCDTPMessageType(CDTP1Message::Type::DATA, "did not receive BOR from " + std::string(sender));
    }
    // Store sequence number and missed messages
    data_transmitter_it->second.missed += data_message.getHeader().getSequenceNumber() - 1 - data_transmitter_it->second.seq;
    data_transmitter_it->second.seq = data_message.getHeader().getSequenceNumber();
    data_transmitter_states_lock.unlock();

    receive_data(std::move(data_message));
}

void ReceiverSatellite::handle_eor_message(CDTP1Message eor_message) {
    const auto sender = eor_message.getHeader().getSender();
    std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
    auto data_transmitter_it = data_transmitter_states_.find(sender);
    // Check that BOR was received
    if(data_transmitter_it == data_transmitter_states_.cend() ||
       data_transmitter_it->second.state != TransmitterState::BOR_RECEIVED) [[unlikely]] {
        throw InvalidCDTPMessageType(CDTP1Message::Type::EOR, "did not receive BOR from " + std::string(sender));
    }
    auto metadata = Dictionary::disassemble(eor_message.getPayload().at(0));

    // Mark run as incomplete if there are missed messages:
    if(data_transmitter_it->second.missed > 0) {
        auto condition_code = CDTP::RunCondition::INCOMPLETE;
        if(is_run_degraded()) {
            condition_code |= CDTP::RunCondition::DEGRADED;
        }
        auto condition_code_it = metadata.find("condition_code");
        if(condition_code_it != metadata.end()) {
            // Add INCOMPLETE flag to existing condition
            condition_code |= condition_code_it->second.get<CDTP::RunCondition>();
            condition_code_it->second = condition_code;
        } else {
            metadata.emplace("condition_code", condition_code);
        }
        // Overwrite existing human-readable run condition
        metadata.insert_or_assign("condition", enum_name(condition_code));
    }

    data_transmitter_it->second.state = TransmitterState::EOR_RECEIVED;
    data_transmitter_states_lock.unlock();

    LOG(cdtp_logger_, INFO) << "Received EOR from " << sender;
    receive_eor(eor_message.getHeader(), std::move(metadata));
}
