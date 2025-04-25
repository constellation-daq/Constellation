/**
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

#include "chirp_mock.hpp"

class CHPMockReceiver
    : public constellation::pools::SubscriberPool<constellation::message::CHP1Message,
                                                  constellation::protocol::CHIRP::ServiceIdentifier::HEARTBEAT> {
public:
    using SubscriberPoolT =
        SubscriberPool<constellation::message::CHP1Message, constellation::protocol::CHIRP::ServiceIdentifier::HEARTBEAT>;
    CHPMockReceiver()
        : SubscriberPoolT("CHP", [this](constellation::message::CHP1Message&& msg) {
              const std::lock_guard last_message_lock {last_message_mutex_};
              last_message_ = std::make_shared<constellation::message::CHP1Message>(std::move(msg));
              last_message_updated_.store(true);
          }) {}

    void waitSubscription() {
        while(!subscribed_.load()) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50ms);
        }
        subscribed_.store(false);
    }
    void waitNextMessage() {
        while(!last_message_updated_.load()) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50ms);
        }
        last_message_updated_.store(false);
    }
    std::shared_ptr<constellation::message::CHP1Message> getLastMessage() {
        const std::lock_guard last_message_lock {last_message_mutex_};
        return last_message_;
    }

protected:
    void host_connected(const constellation::chirp::DiscoveredService& service) final {
        subscribe(service.host_id, "");
        subscribed_.store(true);
    }

private:
    std::atomic_bool subscribed_ {false};
    std::atomic_bool last_message_updated_ {false};
    std::mutex last_message_mutex_;
    std::shared_ptr<constellation::message::CHP1Message> last_message_;
};
