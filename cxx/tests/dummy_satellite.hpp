/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <deque>
#include <stop_token>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp" // IWYU pragma: keep
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/satellite/FSM.hpp"
#include "constellation/satellite/Satellite.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

#include "chirp_mock.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
template <class SatelliteT = constellation::satellite::Satellite> class DummySatelliteNR : public SatelliteT {
public:
    DummySatelliteNR(std::string_view name = "sat1") : SatelliteT("Dummy", name) {
        SatelliteT::support_reconfigure();
        SatelliteT::register_command("my_cmd", "A User Command", {}, &DummySatelliteNR::usr_cmd, this);
        SatelliteT::register_command("_my_hidden_cmd", "A Hidden User Command", {}, &DummySatelliteNR::usr_cmd, this);
        SatelliteT::register_command("my_cmd_arg", "Another User Command", {}, &DummySatelliteNR::usr_cmd_arg, this);
        SatelliteT::register_command(
            "my_cmd_invalid_return", "Invalid User Command", {}, &DummySatelliteNR::usr_cmd_invalid_return, this);
        SatelliteT::register_command(
            "my_cmd_void", "Command without arguments & return", {}, &DummySatelliteNR::usr_cmd_void, this);
        SatelliteT::register_command("my_cmd_state",
                                     "Command for RUN state only",
                                     {constellation::protocol::CSCP::State::RUN},
                                     &DummySatelliteNR::usr_cmd_void,
                                     this);
    }

    void reactFSM(constellation::satellite::FSM::Transition transition,
                  constellation::satellite::FSM::TransitionPayload payload = {},
                  bool progress = true) {
        SatelliteT::getFSM().react(transition, std::move(payload));
        if(progress) {
            progressFsm();
        }
    }

    void progressFsm() {
        auto old_state = SatelliteT::getState();
        LOG(DEBUG) << "Progressing FSM, old state " << old_state << " (" << SatelliteT::getCanonicalName() << ")";
        progress_fsm_ = true;
        // wait for state change
        while(old_state == SatelliteT::getState()) {
            std::this_thread::yield();
        }
        progress_fsm_ = false;
        LOG(DEBUG) << "Progressed FSM, new state " << SatelliteT::getState() << " (" << SatelliteT::getCanonicalName()
                   << ")";
    }

    void setSupportReconfigure(bool support_reconfigure) { SatelliteT::support_reconfigure(support_reconfigure); }
    void setThrowTransitional() { throw_transitional_ = true; }

    void initializing(constellation::config::Configuration& config) override {
        SatelliteT::initializing(config);
        transitional_state("initializing");
    }
    void launching() override {
        SatelliteT::launching();
        transitional_state("launching");
    }
    void landing() override {
        SatelliteT::landing();
        transitional_state("landing");
    }
    void reconfiguring(const constellation::config::Configuration& partial_config) override {
        SatelliteT::reconfiguring(partial_config);
        transitional_state("reconfiguring");
    }
    void starting(std::string_view run_identifier) override {
        SatelliteT::starting(run_identifier);
        transitional_state("starting");
    }
    void stopping() override {
        SatelliteT::stopping();
        transitional_state("stopping");
    }
    void interrupting(constellation::protocol::CSCP::State previous_state, std::string_view reason) override {
        // Note: the default implementation calls `stopping()` and `landing()`, both of which call `transitional_state()`
        progress_fsm_ = true;
        SatelliteT::interrupting(previous_state, reason);
        progress_fsm_ = false;
        transitional_state("interrupting");
    }
    void failure(constellation::protocol::CSCP::State previous_state, std::string_view reason) override {
        SatelliteT::failure(previous_state, reason);
    }

    void skipTransitional(bool skip) { skip_transitional_ = skip; }

    void exit() {
        LOG(DEBUG) << "Exiting satellite";
        skip_transitional_ = true;
        SatelliteT::terminate();
        mocked_services_.clear();
        SatelliteT::join();
    }

    void mockChirpService(constellation::protocol::CHIRP::ServiceIdentifier service) {
        using namespace constellation;
        using enum protocol::CHIRP::ServiceIdentifier;
        const auto canonical_name = SatelliteT::getCanonicalName();
        switch(service) {
        case CONTROL: {
            mocked_services_.emplace_back(canonical_name, service, SatelliteT::getCommandPort());
            break;
        }
        case HEARTBEAT: {
            mocked_services_.emplace_back(canonical_name, service, SatelliteT::getHeartbeatPort());
            break;
        }
        case MONITORING: {
            mocked_services_.emplace_back(canonical_name, service, utils::ManagerLocator::getSinkManager().getCMDPPort());
            break;
        }
        case DATA: {
            if constexpr(std::same_as<SatelliteT, satellite::TransmitterSatellite>) {
                mocked_services_.emplace_back(canonical_name, service, SatelliteT::getDataPort());
            }
            break;
        }
        default: break;
        }
    }

protected:
    void transitional_state(std::string_view state) {
        LOG(TRACE) << "Entering transitional state " << state << " (" << SatelliteT::getCanonicalName() << ")";
        if(skip_transitional_) {
            LOG(TRACE) << "Skipping transitional state " << state << " (" << SatelliteT::getCanonicalName() << ")";
            return;
        }
        while(!progress_fsm_) {
            if(throw_transitional_) {
                throw_transitional_ = false;
                throw constellation::utils::Exception("Throwing in transitional state as requested");
            }
            std::this_thread::yield();
        }
        LOG(TRACE) << "Leaving transitional state " << state << " (" << SatelliteT::getCanonicalName() << ")";
        SatelliteT::submit_status("Finished with transitional state " + std::string(state));
    }

private:
    // NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)
    int usr_cmd() { return 2; }
    int usr_cmd_arg(int a) { return 2 * a; }
    std::array<int, 1> usr_cmd_invalid_return() { return {2}; }
    void usr_cmd_void() { value_ = 3; };
    int value_ {2};
    // NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

private:
    std::atomic_bool progress_fsm_ {false};
    std::atomic_bool skip_transitional_ {false};
    std::atomic_bool throw_transitional_ {false};
    std::deque<MockedChirpService> mocked_services_;
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
template <class SatelliteT = constellation::satellite::Satellite>
class DummySatellite : public DummySatelliteNR<SatelliteT> {
public:
    DummySatellite(std::string_view name = "sat1") : DummySatelliteNR<SatelliteT>(name) {}

    void running(const std::stop_token& stop_token) override {
        SatelliteT::running(stop_token);
        LOG(TRACE) << "Entering running function (" << SatelliteT::getCanonicalName() << ")";
        while(!stop_token.stop_requested()) {
            if(throw_running_) {
                throw_running_ = false;
                throw constellation::utils::Exception("Throwing in running as requested");
            }
            std::this_thread::yield();
        }
        LOG(TRACE) << "Leaving running function (" << SatelliteT::getCanonicalName() << ")";
        SatelliteT::submit_status("Finished with running function");
    }

    void setThrowRunning() { throw_running_ = true; }

private:
    std::atomic_bool throw_running_ {false};
};
