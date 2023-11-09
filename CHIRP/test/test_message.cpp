#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "CHIRP/exceptions.hpp"
#include "CHIRP/Message.hpp"

using namespace cnstln::CHIRP;

int test_message_md5_hash() {
    int fails = 0;
    // Test values from RFC 1321 reference implementation
    fails += MD5Hash("").to_string() == "d41d8cd98f00b204e9800998ecf8427e" ? 0 : 1;
    fails += MD5Hash("a").to_string() == "0cc175b9c0f1b6a831c399e269772661" ? 0 : 1;
    fails += MD5Hash("abc").to_string() == "900150983cd24fb0d6963f7d28e17f72" ? 0 : 1;
    fails += MD5Hash("message digest").to_string() == "f96b697d7cb7938d525a2f31aaf161d0" ? 0 : 1;
    fails += MD5Hash("abcdefghijklmnopqrstuvwxyz").to_string() == "c3fcd3d76192e4007dfb496cca67e13b" ? 0 : 1;
    fails += MD5Hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789").to_string() == "d174ab98d277d9f5a5611c2c9f419d9f" ? 0 : 1;
    fails += MD5Hash("12345678901234567890123456789012345678901234567890123456789012345678901234567890").to_string() == "57edf4a22be3c955ac49da2e2107b67a" ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_message_md5_sort() {
    int fails = 0;
    fails += MD5Hash("a") < MD5Hash("a") ? 1 : 0;
    fails += MD5Hash("a") < MD5Hash("b") ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_message_assemble() {
    std::vector<std::uint8_t> msg_data {};
    // Success on correct size
    msg_data.resize(MESSAGE_LENGTH);
    AssembledMessage {msg_data};
    // Exception on incorrect size
    msg_data.resize(MESSAGE_LENGTH + 1);
    int ret = 1;
    try {
        AssembledMessage {msg_data};
    }
    catch (const DecodeError& error) {
        if (std::strcmp(error.what(), ("Message length is not " + std::to_string(MESSAGE_LENGTH) + " bytes").c_str()) == 0) {
            ret = 0;
        }
    }
    return ret;
}

int test_message_reconstructed() {
    auto msg = Message(OFFER, "group", "host", CONTROL, 47890);
    auto asm_msg = msg.Assemble();
    auto msg_reconstructed = Message(asm_msg);
    int fails = 0;
    fails += msg.GetType() == msg_reconstructed.GetType() ? 0 : 1;
    fails += msg.GetGroupID() == msg_reconstructed.GetGroupID() ? 0 : 1;
    fails += msg.GetHostID() == msg_reconstructed.GetHostID() ? 0 : 1;
    fails += msg.GetServiceIdentifier() == msg_reconstructed.GetServiceIdentifier() ? 0 : 1;
    fails += msg.GetPort() == msg_reconstructed.GetPort() ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_message_construct_invalid_chirpv1() {
    auto msg = Message(REQUEST, "group", "host", HEARTBEAT, 0);
    auto asm_msg = msg.Assemble();
    asm_msg[0] = 'X';
    int ret = 1;
    try {
        Message {asm_msg};
    }
    catch (const DecodeError& error) {
        if (std::strcmp(error.what(), "Not a CHIRP v1 broadcast") == 0) {
            ret = 0;
        }
    }
    return ret;
}

int test_message_construct_invalid_type() {
    auto msg = Message(static_cast<MessageType>(255), "group", "host", DATA, 0);
    auto asm_msg = msg.Assemble();
    int ret = 1;
    try {
        Message {asm_msg};
    }
    catch (const DecodeError& error) {
        if (std::strcmp(error.what(), "Message Type invalid") == 0) {
            ret = 0;
        }
    }
    return ret;
}

int test_message_construct_invalid_service() {
    auto msg = Message(OFFER, "group", "host", static_cast<ServiceIdentifier>(255), 12345);
    auto asm_msg = msg.Assemble();
    int ret = 1;
    try {
        Message {asm_msg};
    }
    catch (const DecodeError& error) {
        if (std::strcmp(error.what(), "Service Identifier invalid") == 0) {
            ret = 0;
        }
    }
    return ret;
}

int main() {
    int ret = 0;
    int ret_test = 0;

    // test_message_md5_hash
    std::cout << "test_message_md5_hash...                     " << std::flush;
    ret_test = test_message_md5_hash();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_message_md5_sort
    std::cout << "test_message_md5_sort...                     " << std::flush;
    ret_test = test_message_md5_sort();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_message_assemble
    std::cout << "test_message_assemble...                    " << std::flush;
    ret_test = test_message_assemble();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_message_reconstructed
    std::cout << "test_message_reconstructed...                " << std::flush;
    ret_test = test_message_reconstructed();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_message_construct_invalid_chirpv1
    std::cout << "test_message_construct_invalid_chirpv1...    " << std::flush;
    ret_test = test_message_construct_invalid_chirpv1();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_message_construct_invalid_type
    std::cout << "test_message_construct_invalid_type...       " << std::flush;
    ret_test = test_message_construct_invalid_type();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_message_construct_invalid_service
    std::cout << "test_message_construct_invalid_service...    " << std::flush;
    ret_test = test_message_construct_invalid_service();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    if (ret == 0) {
        std::cout << "\nAll tests passed" << std::endl;
    }
    else {
        std::cout << "\n" << ret << " tests failed" << std::endl;
    }
    return ret;
}
