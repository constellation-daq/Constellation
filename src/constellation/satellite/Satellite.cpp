/**
 * @file
 * @brief Implementation of Satellite class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Satellite.hpp"

#include <any>
#include <stop_token>

#include "constellation/core/logging/log.hpp"

using namespace constellation::satellite;

#define LOGGER logger_

Satellite::Satellite() : logger_("SATELLITE") {}

void Satellite::initializing(const std::stop_token& /* stop_token */, const std::any& /* config */) {
    LOG(INFO) << "Initializing - empty";
}

void Satellite::launching(const std::stop_token& /* stop_token */) {
    LOG(INFO) << "Launching - empty";
}

void Satellite::landing(const std::stop_token& /* stop_token */) {
    LOG(INFO) << "Landing - empty";
}

void Satellite::reconfiguring(const std::stop_token& /* stop_token */, const std::any& /* partial_config */) {
    // TODO(stephan.lachnit): throw if not supported
    LOG(INFO) << "Reconfiguring - empty";
}

void Satellite::starting(const std::stop_token& /* stop_token */, std::uint32_t run_number) {
    LOG(INFO) << "Starting run " << run_number << " - empty";
}

void Satellite::stopping(const std::stop_token& /* stop_token */) {
    LOG(INFO) << "Stopping - empty";
}

void Satellite::running(const std::stop_token& /* stop_token */) {
    LOG(INFO) << "Running - empty";
}

void Satellite::interrupting(const std::stop_token& /* stop_token */, State /* previous_state */) {
    LOG(INFO) << "Interrupting - empty";
}

void Satellite::on_failure(const std::stop_token& /* stop_token */, State /* previous_state */) {
    LOG(INFO) << "Failure - empty";
}
