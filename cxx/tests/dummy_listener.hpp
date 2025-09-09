/**
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/CMDPListener.hpp"

class DummyListener : public constellation::listener::CMDPListener {
public:
    DummyListener(std::string_view name = "DUMMY")
        : constellation::listener::CMDPListener(
              name, [this](constellation::message::CMDP1Message&& message) { handle_message(std::move(message)); }) {}

    constellation::message::CMDP1Message popNextMessage() {
        while(true) {
            std::unique_lock messages_lock {messages_mutex_};
            if(!messages_.empty()) {
                break;
            }
            messages_lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        const std::lock_guard messages_lock {messages_mutex_};
        auto msg = std::move(messages_.front());
        messages_.pop_front();
        return msg;
    }

private:
    void handle_message(constellation::message::CMDP1Message&& message) {
        LOG(DEBUG) << "Received message with topic " << constellation::utils::quote(message.getTopic()) << " from "
                   << constellation::utils::quote(message.getHeader().getSender());
        const std::lock_guard messages_lock {messages_mutex_};
        messages_.emplace_back(std::move(message));
    }

private:
    std::deque<constellation::message::CMDP1Message> messages_;
    std::mutex messages_mutex_;
};
