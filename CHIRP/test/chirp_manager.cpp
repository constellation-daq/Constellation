#include <charconv>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "asio.hpp"
#include "magic_enum.hpp"

#include "CHIRP/Manager.hpp"
#include "CHIRP/Message.hpp"

using namespace cnstln::CHIRP;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

enum class Command {
    list_registered_services,
    list_discovered_services,
    register_service,
    unregister_service,
    register_callback,
    unregister_callback,
    request,
    reset,
};
using enum Command;

void discover_callback(DiscoveredService service, bool depart, void*) {
    std::cout << "Callback:\n"
              << " Service " << std::left << std::setw(10) << magic_enum::enum_name(service.identifier)
              << " Port " << std::setw(5) << service.port
              << " Host " << service.host_hash.to_string()
              << " IP " << std::left << std::setw(15) << service.ip.to_string()
              << (depart ? " DEPART" : " OFFER")
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Specify satellite name, brd address, group name and any address via cmdline
    auto name = "control"s;
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

    Manager manager {brd_address, any_address, group, name};

    std::cout << "Commands: "
              << "\n list_registered_services"
              << "\n list_discovered_services <ServiceIdentifier>"
              << "\n register_service <ServiceIdentifier:CONTROL> <Port:23999>"
              << "\n unregister_service <ServiceIdentifier:CONTROL> <Port:23999>"
              << "\n register_callback <ServiceIdentifier:CONTROL>"
              << "\n unregister_callback <ServiceIdentifier:CONTROL>"
              << "\n request <ServiceIdentifier:CONTROL>"
              << "\n reset"
              << std::endl;
    manager.Start();

    bool quit = false;
    while (!quit) {
        std::string cmd_input {};
        std::getline(std::cin, cmd_input);

        // Split in vector of string views
        std::vector<std::string_view> cmd_split {};
        for (const auto word_range : std::ranges::views::split(cmd_input, " "sv)) {
            cmd_split.emplace_back(&*word_range.begin(), std::ranges::distance(word_range));
        }

        // If not a command, continue
        if (cmd_split.size() < 1) {
            continue;
        }
        auto cmd_opt = magic_enum::enum_cast<Command>(cmd_split[0]);
        if (!cmd_opt.has_value()) {
            std::cout << std::quoted(cmd_split[0]) << " is not a valid command" << std::endl;
            continue;
        }
        auto cmd = cmd_opt.value();

        // List registered services
        if (cmd == list_registered_services) {
            auto registered_services = manager.GetRegisteredServices();
            std::cout << " Registered Services:";
            for (const auto& service : registered_services) {
                std::cout << "\n Service " << std::left << std::setw(10) << magic_enum::enum_name(service.identifier)
                          << " Port " << std::setw(5) << service.port;
            }
            std::cout << std::endl;
        }
        // List discovered services
        else if (cmd == list_discovered_services) {
            std::optional<ServiceIdentifier> service_opt {std::nullopt};
            if (cmd_split.size() >= 2) {
                service_opt = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]);
            }
            auto discovered_services = service_opt.has_value() ? manager.GetDiscoveredServices(service_opt.value())
                                                               : manager.GetDiscoveredServices();
            std::cout << " Discovered Services:";
            for (const auto& service : discovered_services) {
                std::cout << "\n Service " << std::left << std::setw(10) << magic_enum::enum_name(service.identifier)
                          << " Port " << std::setw(5) << service.port
                          << " Host " << service.host_hash.to_string()
                          << " IP " << std::left << std::setw(15) << service.ip.to_string();
            }
            std::cout << std::endl;
        }
        // Register or unregister a service
        else if (cmd == register_service || cmd == unregister_service) {
            ServiceIdentifier service {CONTROL};
            if (cmd_split.size() >= 2) {
                service = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]).value_or(CONTROL);
            }
            Port port {23999};
            if (cmd_split.size() >= 3) {
                std::from_chars(cmd_split[2].data(), cmd_split[2].data() + cmd_split[2].size(), port);
            }
            if (cmd == register_service) {
                auto ret = manager.RegisterService(service, port);
                if (ret) {
                    std::cout << " Registered Service " << std::left << std::setw(10) << magic_enum::enum_name(service)
                              << " Port " << std::setw(5) << port << std::endl;
                }
            }
            else {
                auto ret = manager.UnregisterService(service, port);
                if (ret) {
                    std::cout << " Unregistered Service " << std::left << std::setw(10) << magic_enum::enum_name(service)
                              << " Port " << std::setw(5) << port << std::endl;
                }
            }
        }
        // Register of unregister callback
        else if (cmd == register_callback || cmd == unregister_callback) {
            ServiceIdentifier service {CONTROL};
            if (cmd_split.size() >= 2) {
                service = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]).value_or(CONTROL);
            }
            if (cmd == register_callback) {
                auto ret = manager.RegisterDiscoverCallback(&discover_callback, service, nullptr);
                if (ret) {
                    std::cout << " Registered Callback for " << magic_enum::enum_name(service) << std::endl;
                }
            }
            else {
                auto ret = manager.UnregisterDiscoverCallback(&discover_callback, service, nullptr);
                if (ret) {
                    std::cout << " Unregistered Callback for " << magic_enum::enum_name(service) << std::endl;
                }
            }
        }
        // Send CHIRP request
        else if (cmd == request) {
            ServiceIdentifier service {CONTROL};
            if (cmd_split.size() >= 2) {
                service = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]).value_or(CONTROL);
            }
            manager.SendRequest(service);
            std::cout << " Sent Request for " << magic_enum::enum_name(service) << std::endl;
        }
        // Reset
        else {
            manager.UnregisterDiscoverCallbacks();
            manager.UnregisterServices();
            manager.ForgetDiscoveredServices();
        }
    }
}
