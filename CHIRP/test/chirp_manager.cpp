#include <string>

#include "asio.hpp"

#include "CHIRP/Manager.hpp"

using namespace cnstln::CHIRP;
using namespace std::literals::string_literals;

int main(int argc, char* argv[]) {
    // Specify satellite name, brd address, group name and any address via cmdline
    auto name = "satname"s;
    asio::ip::address brd_address = asio::ip::address_v4::broadcast();
    auto group = "cnstln1"s;
    asio::ip::address any_address = asio::ip::address_v4::any();
    if (argc >= 2) {
        name = argv[1];
    }
    if (argc >= 3) {
        brd_address = asio::ip::make_address(argv[2]);
    }
    if (argc >= 4) {
        group = argv[3];
    }
    if (argc >= 5) {
        any_address = asio::ip::make_address(argv[4]);
    }

    asio::io_context io_context {};
    {
        // TODO: add brd and any address
        Manager manager {io_context, brd_address, any_address, group, name};

        // TODO: this is just a demo, make it a proper CLI
        manager.RegisterService({CONTROL, 23999});
        manager.RegisterService({DATA, 24000});
    }
}
