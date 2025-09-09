/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"

#include "chirp_mock.hpp"
#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

class CSCPSender {
public:
    CSCPSender(Port port) : req_socket_(*global_zmq_context(), zmq::socket_type::req) {
        req_socket_.connect("tcp://127.0.0.1:" + to_string(port));
    }
    void send(std::span<const std::byte> message, zmq::send_flags send_flags = zmq::send_flags::none) {
        zmq::message_t zmq_msg {message.data(), message.size()};
        req_socket_.send(zmq_msg, send_flags);
    }
    void send(CSCP1Message& message) { message.assemble().send(req_socket_); }
    void sendCommand(std::string command) {
        auto msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, std::move(command)});
        send(msg);
    }
    CSCP1Message recv() {
        zmq::multipart_t zmq_msgs {};
        zmq_msgs.recv(req_socket_);
        return CSCP1Message::disassemble(zmq_msgs);
    }

private:
    zmq::socket_t req_socket_;
};

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Standard commands", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // get_name
    sender.sendCommand("get_name");
    auto recv_msg_get_name = sender.recv();
    REQUIRE(recv_msg_get_name.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_name.getVerb().second), Equals(satellite.getCanonicalName()));
    REQUIRE(!recv_msg_get_name.hasPayload());

    // get_commands
    sender.sendCommand("get_commands");
    auto recv_msg_get_commands = sender.recv();
    REQUIRE(recv_msg_get_commands.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_commands.getVerb().second), EndsWith("commands known, list attached in payload"));
    REQUIRE(recv_msg_get_commands.hasPayload());
    const auto get_commands_dict = Dictionary::disassemble(recv_msg_get_commands.getPayload());
    REQUIRE(get_commands_dict.contains("get_commands"));
    REQUIRE(get_commands_dict.at("stop").get<std::string>() == "Stop run");
    REQUIRE(get_commands_dict.contains("my_cmd"));
    REQUIRE_THAT(std::get<std::string>(get_commands_dict.at("my_cmd")),
                 Equals("A User Command\nThis command requires 0 arguments.\nThis command can be called in all states."));
    REQUIRE(get_commands_dict.contains("my_cmd_state"));
    REQUIRE_THAT(
        std::get<std::string>(get_commands_dict.at("my_cmd_state")),
        Equals("Command for RUN state only\nThis command requires 0 arguments.\nThis command can only be called in the "
               "following states: RUN"));

    // get_state
    sender.sendCommand("get_state");
    auto recv_msg_get_state = sender.recv();
    REQUIRE(recv_msg_get_state.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_state.getVerb().second), Equals("NEW"));
    const auto& recv_get_state_payload = recv_msg_get_state.getPayload();
    auto recv_get_state = msgpack_unpack_to<std::underlying_type_t<CSCP::State>>(
        to_char_ptr(recv_get_state_payload.span().data()), recv_get_state_payload.span().size());
    REQUIRE(recv_get_state == std::to_underlying(CSCP::State::NEW));

    // get_role
    sender.sendCommand("get_role");
    auto recv_msg_get_role = sender.recv();
    REQUIRE(recv_msg_get_role.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_role.getVerb().second), Equals("DYNAMIC"));
    const auto& recv_msg_get_role_payload = recv_msg_get_role.getPayload();
    auto recv_get_role = msgpack_unpack_to<std::underlying_type_t<CHP::MessageFlags>>(
        to_char_ptr(recv_msg_get_role_payload.span().data()), recv_msg_get_role_payload.span().size());
    REQUIRE(recv_get_role == std::to_underlying(flags_from_role(CHP::Role::DYNAMIC)));

    // get_status
    sender.sendCommand("get_status");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals(""));
    REQUIRE(!recv_msg_get_status.hasPayload());

    // get_config
    sender.sendCommand("get_config");
    auto recv_msg_get_config = sender.recv();
    REQUIRE(recv_msg_get_config.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_config.getVerb().second),
                 Equals("0 configuration keys, dictionary attached in payload"));
    REQUIRE(recv_msg_get_config.hasPayload());
    const auto config = Configuration(Dictionary::disassemble(recv_msg_get_config.getPayload()));
    REQUIRE(config.size() == 0);
    // TODO(stephan.lachnit): test with a non-empty configuration

    // get_version
    sender.sendCommand("get_version");
    auto recv_msg_get_version = sender.recv();
    REQUIRE(recv_msg_get_version.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_version.getVerb().second), Equals(CNSTLN_VERSION));
    REQUIRE_FALSE(recv_msg_get_version.hasPayload());

    satellite.exit();
}

TEST_CASE("Hidden commands", "[satellite]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Create and start satellites
    DummySatellite satelliteA {"a"};
    DummySatellite satelliteB {"b"};
    satelliteB.mockChirpService(CHIRP::HEARTBEAT);
    const auto satelliteB_md5 = MD5Hash(satelliteB.getCanonicalName()).to_string();

    // Create sender
    CSCPSender sender {satelliteA.getCommandPort()};

    // _get_commands
    sender.sendCommand("_get_commands");
    auto recv_msg_get_commands = sender.recv();
    REQUIRE(recv_msg_get_commands.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_commands.getVerb().second), EndsWith("commands known, list attached in payload"));
    REQUIRE(recv_msg_get_commands.hasPayload());
    const auto get_commands_dict = Dictionary::disassemble(recv_msg_get_commands.getPayload());
    REQUIRE(get_commands_dict.contains("_get_commands"));
    REQUIRE_THAT(get_commands_dict.at("_interrupt").get<std::string>(),
                 Equals("Send interrupt signal to satellite to transition to SAFE mode"));
    REQUIRE_THAT(get_commands_dict.at("_failure").get<std::string>(),
                 Equals("Send failure signal to satellite to transition to ERROR mode"));
    REQUIRE(get_commands_dict.contains("_my_hidden_cmd"));
    REQUIRE_THAT(
        std::get<std::string>(get_commands_dict.at("_my_hidden_cmd")),
        Equals("A Hidden User Command\nThis command requires 0 arguments.\nThis command can be called in all states."));

    // _get_services
    sender.sendCommand("_get_services");
    auto recv_msg_get_services = sender.recv();
    REQUIRE(recv_msg_get_services.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_services.getVerb().second), Equals("2 services offered, list attached in payload"));
    const auto get_services_dict = Dictionary::disassemble(recv_msg_get_services.getPayload());
    REQUIRE(get_services_dict.contains("CONTROL"));
    REQUIRE(get_services_dict.contains("HEARTBEAT"));

    // _get_remotes
    sender.sendCommand("_get_remotes");
    auto recv_msg_get_remotes = sender.recv();
    REQUIRE(recv_msg_get_remotes.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_remotes.getVerb().second),
                 Equals("1 remote services registered, list attached in payload"));
    const auto get_remotes_dict = Dictionary::disassemble(recv_msg_get_remotes.getPayload());
    REQUIRE(get_remotes_dict.at(satelliteB_md5).get<std::vector<std::string>>().size() == 1);
    REQUIRE_THAT(get_remotes_dict.at(satelliteB_md5).get<std::vector<std::string>>().at(0), StartsWith("HEARTBEAT @"));

    satelliteA.exit();
    satelliteB.exit();
}

TEST_CASE("Satellite name", "[satellite]") {
    class InvalidSatellite : public Satellite {
    public:
        InvalidSatellite() : Satellite("Invalid", "invalid_satellite&name") {}
    };
    REQUIRE_THROWS_MATCHES(InvalidSatellite(), RuntimeError, Message("Satellite name is invalid"));
}

TEST_CASE("User commands", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // my_cmd user command
    sender.sendCommand("my_cmd");
    auto recv_msg_usr_cmd = sender.recv();
    REQUIRE(recv_msg_usr_cmd.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd.getVerb().second), Equals("Command returned: 2"));
    REQUIRE(recv_msg_usr_cmd.hasPayload());
    const auto& usrmsgpayload = recv_msg_usr_cmd.getPayload();
    const auto usrpayload = msgpack_unpack_to<int>(to_char_ptr(usrmsgpayload.span().data()), usrmsgpayload.span().size());
    REQUIRE(usrpayload == 2);

    // my_cmd user command is case insensitive
    sender.sendCommand("mY_cMd");
    auto recv_msg_usr_cmd_case = sender.recv();
    REQUIRE(recv_msg_usr_cmd_case.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_case.getVerb().second), Equals("Command returned: 2"));
    REQUIRE(recv_msg_usr_cmd_case.hasPayload());
    const auto& usrmsgpayload_case = recv_msg_usr_cmd_case.getPayload();
    const auto usrpayload_case =
        msgpack_unpack_to<int>(to_char_ptr(usrmsgpayload_case.span().data()), usrmsgpayload_case.span().size());
    REQUIRE(usrpayload_case == 2);

    // my_usr_cmd_arg with argument as payload
    auto usr_cmd_arg_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    msgpack::sbuffer sbuf {};
    List args {};
    args.push_back(4);
    msgpack::pack(sbuf, args);
    usr_cmd_arg_msg.addPayload(std::move(sbuf));
    sender.send(usr_cmd_arg_msg);

    auto recv_msg_usr_cmd_arg = sender.recv();
    REQUIRE(recv_msg_usr_cmd_arg.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_arg.getVerb().second), Equals("Command returned: 8"));
    REQUIRE(recv_msg_usr_cmd_arg.hasPayload());
    const auto& usrargmsgpayload = recv_msg_usr_cmd_arg.getPayload();
    const auto usrargpayload =
        msgpack_unpack_to<int>(to_char_ptr(usrargmsgpayload.span().data()), usrargmsgpayload.span().size());
    REQUIRE(usrargpayload == 8);

    // my_cmd_void user command without arguments and return value
    sender.sendCommand("my_cmd_void");
    auto recv_msg_usr_cmd_void = sender.recv();
    REQUIRE(recv_msg_usr_cmd_void.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_void.getVerb().second), Equals("Command returned: NIL"));
    REQUIRE(!recv_msg_usr_cmd_void.hasPayload());

    satellite.exit();
}

TEST_CASE("Case insensitive", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // get_name with non-lower-case case
    sender.sendCommand("GeT_nAmE");
    auto recv_msg_get_name = sender.recv();
    REQUIRE(recv_msg_get_name.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_name.getVerb().second), Equals(satellite.getCanonicalName()));
    REQUIRE(!recv_msg_get_name.hasPayload());

    // my_cmd user command
    sender.sendCommand("mY_cMd");
    auto recv_msg_usr_cmdn = sender.recv();
    REQUIRE(recv_msg_usr_cmdn.getVerb().first == CSCP1Message::Type::SUCCESS);

    satellite.exit();
}

TEST_CASE("Transitions", "[satellite]") {
    // Create and start satellite with FSM
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send initialize
    auto initialize_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "initialize"});
    initialize_msg.addPayload(Dictionary().assemble());
    sender.send(initialize_msg);

    // Check reply
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_initialize.getVerb().second), Equals("Transition initialize is being initiated"));

    // Check state
    satellite.progressFsm();
    sender.sendCommand("get_state");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("INIT"));

    // Check status
    REQUIRE_THAT(to_string(satellite.getStatus()), Equals("Finished with transitional state initializing"));

    satellite.exit();
}

TEST_CASE("Shutdown", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send initialize
    auto initialize_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "initialize"});
    initialize_msg.addPayload(Dictionary().assemble());
    sender.send(initialize_msg);
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::SUCCESS);
    satellite.progressFsm();

    // Send launch
    auto launch_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "launch"});
    sender.send(launch_msg);
    auto recv_msg_launch = sender.recv();
    REQUIRE(recv_msg_launch.getVerb().first == CSCP1Message::Type::SUCCESS);
    satellite.progressFsm();

    // Try shutdown & fail
    auto shutdown1_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "shutdown"});
    sender.send(shutdown1_msg);
    auto recv_msg_shutdown1 = sender.recv();
    REQUIRE(recv_msg_shutdown1.getVerb().first == CSCP1Message::Type::INVALID);
    REQUIRE_THAT(to_string(recv_msg_shutdown1.getVerb().second),
                 Equals("Satellite cannot be shut down from current state ORBIT"));

    // Send land
    auto land_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "land"});
    sender.send(land_msg);
    auto recv_msg_land = sender.recv();
    REQUIRE(recv_msg_land.getVerb().first == CSCP1Message::Type::SUCCESS);
    satellite.progressFsm();

    // Try shutdown & succeed
    auto shutdown2_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "shutdown"});
    sender.send(shutdown2_msg);
    auto recv_msg_shutdown2 = sender.recv();
    REQUIRE(recv_msg_shutdown2.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_shutdown2.getVerb().second), Equals("Shutting down satellite"));

    satellite.join();
}

TEST_CASE("Catch unknown command", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send with unexpected message type
    auto wrong_type_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "get_names"});
    sender.send(wrong_type_msg);
    auto recv_msg_wrong_type = sender.recv();
    REQUIRE(recv_msg_wrong_type.getVerb().first == CSCP1Message::Type::UNKNOWN);
    REQUIRE_THAT(to_string(recv_msg_wrong_type.getVerb().second), Equals("Command `get_names` is not known"));

    satellite.exit();
}

TEST_CASE("Catch unexpected message type", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send with unexpected message type
    auto wrong_type_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::SUCCESS, "get_name"});
    sender.send(wrong_type_msg);
    auto recv_msg_wrong_type = sender.recv();
    REQUIRE(recv_msg_wrong_type.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_wrong_type.getVerb().second), Equals("Can only handle CSCP messages with REQUEST type"));

    satellite.exit();
}

TEST_CASE("Catch invalid protocol", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send with invalid protocol
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "INVALID");
    msgpack::pack(sbuf, "cscp_sender");
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    msgpack::pack(sbuf, Dictionary());
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()}, zmq::send_flags::sndmore);
    sbuf.clear();
    msgpack::pack(sbuf, std::to_underlying(CSCP1Message::Type::REQUEST));
    msgpack::pack(sbuf, "get_name");
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()});

    auto recv_msg_invalid_proto = sender.recv();
    REQUIRE(recv_msg_invalid_proto.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_invalid_proto.getVerb().second), Equals("Invalid protocol identifier `INVALID`"));

    satellite.exit();
}

TEST_CASE("Catch unexpected protocol", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send with unexpected protocol
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "CMDP\x01");
    msgpack::pack(sbuf, "cscp_sender");
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    msgpack::pack(sbuf, Dictionary());
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()}, zmq::send_flags::sndmore);
    sbuf.clear();
    msgpack::pack(sbuf, std::to_underlying(CSCP1Message::Type::REQUEST));
    msgpack::pack(sbuf, "get_name");
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()});

    auto recv_msg_wrong_proto = sender.recv();
    REQUIRE(recv_msg_wrong_proto.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_wrong_proto.getVerb().second),
                 Equals("Received protocol `CMDP1` does not match expected identifier `CSCP1`"));

    satellite.exit();
}

TEST_CASE("Catch incorrect payload", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send initialize
    auto initialize_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "initialize"});
    initialize_msg.addPayload({"dummy_payload"s});
    sender.send(initialize_msg);

    // Check reply
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_initialize.getVerb().second),
                 Equals("Transition initialize received incorrect payload"));

    // Check state
    sender.sendCommand("get_state");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("NEW"));

    satellite.exit();
}

TEST_CASE("Catch invalid user command registrations", "[satellite]") {
    class MySatellite : public DummySatellite<> {
    public:
        void registerCommand(std::string_view name) {
            register_command(name, "A User Command", {}, []() {});
        }
    };

    auto my_satellite = MySatellite();

    REQUIRE_THROWS_MATCHES(my_satellite.registerCommand(""), LogicError, Message("Command name `` is invalid"));

    REQUIRE_THROWS_MATCHES(my_satellite.registerCommand("command_with_amper&sand"),
                           LogicError,
                           Message("Command name `command_with_amper&sand` is invalid"));

    my_satellite.registerCommand("my_cmd_CaSiNg");
    REQUIRE_THROWS_MATCHES(
        my_satellite.registerCommand("my_cmd_casing"), LogicError, Message("Command `my_cmd_casing` is already registered"));

    REQUIRE_THROWS_MATCHES(my_satellite.registerCommand("initialize"),
                           LogicError,
                           Message("Satellite transition command with this name exists"));

    REQUIRE_THROWS_MATCHES(my_satellite.registerCommand("get_commands"),
                           LogicError,
                           Message("Standard satellite command with this name exists"));
}

TEST_CASE("Catch incorrect user command arguments", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // my_usr_cmd_arg with wrong payload encoding
    auto nolist_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    auto nolist_payload = PayloadBuffer("dummy payload"s);
    nolist_msg.addPayload(std::move(nolist_payload));
    sender.send(nolist_msg);

    auto recv_msg_nolist = sender.recv();
    REQUIRE(recv_msg_nolist.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_nolist.getVerb().second), Equals("Could not convert command payload to argument list"));

    // my_usr_cmd_arg with wrong argument type
    auto wrongarg_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    List args {};
    args.push_back(std::chrono::system_clock::now());
    wrongarg_msg.addPayload(args.assemble());
    sender.send(wrongarg_msg);

    auto recv_msg_wrongarg = sender.recv();
    REQUIRE(recv_msg_wrongarg.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_wrongarg.getVerb().second),
                 StartsWith("Mismatch of argument type `int` to provided type `std::chrono::system_clock::time_point"));

    // my_usr_cmd_arg with wrong number of argument
    auto manyarg_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    List manyargs {};
    manyargs.push_back(3);
    manyargs.push_back(4);
    manyarg_msg.addPayload(manyargs.assemble());
    sender.send(manyarg_msg);

    auto recv_msg_manyarg = sender.recv();
    REQUIRE(recv_msg_manyarg.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_manyarg.getVerb().second),
                 Equals("Command `my_cmd_arg` expects 1 arguments but 2 given"));

    // my_usr_state from wrong state
    sender.sendCommand("my_cmd_state");
    auto recv_msg_usr_cmd_state = sender.recv();
    REQUIRE(recv_msg_usr_cmd_state.getVerb().first == CSCP1Message::Type::INVALID);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_state.getVerb().second),
                 Equals("Command my_cmd_state cannot be called in state NEW"));
    REQUIRE(!recv_msg_usr_cmd_state.hasPayload());

    satellite.exit();
}

TEST_CASE("Catch incorrect user command return value", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // my_usr_cmd_arg with wrong payload encoding
    auto invalid_return = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_invalid_return"});
    sender.send(invalid_return);

    auto recv_msg_invalid_return = sender.recv();
    REQUIRE(recv_msg_invalid_return.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_invalid_return.getVerb().second),
                 Equals("Error casting function return type `std::array<int, 1>` to dictionary value"));

    satellite.exit();
}

TEST_CASE("Catch wrong number of frames", "[satellite]") {
    // Create and start satellite
    DummySatellite satellite {};

    // Create sender
    CSCPSender sender {satellite.getCommandPort()};

    // Send with wrong number of frames
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "CSCP\x01");
    msgpack::pack(sbuf, "cscp_sender");
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    msgpack::pack(sbuf, Dictionary());
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()});

    auto recv_msg_wrong_frames = sender.recv();
    REQUIRE(recv_msg_wrong_frames.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_wrong_frames.getVerb().second),
                 Equals("Error decoding CSCP1 message: Incorrect number of message frames"));

    satellite.exit();
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
