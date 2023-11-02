#include <array>
#include <vector>
#include <cstdint>
#include <string>
#include <string_view>

namespace CHIRP {
    constexpr char CHIRP_VERSION = '\x01';
    constexpr std::size_t MESSAGE_LENGTH = 42;

    enum class MessageType : char {
        REQUEST = '\x01',
        OFFER = '\x02',
    };

    enum class ServiceIdentifier : char {
        CONTROL = '\x01',
        HEARTBEAT = '\x02',
        MONITORING = '\x03',
        DATA = '\x04',
    };

    using Port = std::uint16_t;

    class MD5Hash : public std::array<std::uint8_t, 16> {
    public:
        MD5Hash() {}
        MD5Hash(std::string_view string);
        std::string toString() const;
    };

    class Message {
    public:
        Message(std::string_view group, std::string_view name, MessageType type, ServiceIdentifier service, Port port);
        Message(MD5Hash group_hash, MD5Hash name_hash, MessageType type, ServiceIdentifier service, Port port);
        Message(std::vector<char>& assembled_message);
        Message(std::array<std::uint8_t, MESSAGE_LENGTH>& assembled_message);

        MD5Hash GetGroupHash() const { return group_hash_; }
        MD5Hash GetNameHash() const { return name_hash_; }
        MessageType GetType() const { return type_; }
        ServiceIdentifier GetServiceIdentifier() const { return service_; }
        Port GetPort() const { return port_; }

        std::array<std::uint8_t, MESSAGE_LENGTH> Assemble() const;

    private:
        MD5Hash group_hash_;
        MD5Hash name_hash_;
        MessageType type_;
        ServiceIdentifier service_;
        Port port_;
    };
}
