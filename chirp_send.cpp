#include "BroadcastSend.hpp"
#include "CHIRP/Message.hpp"

using namespace CHIRP;

int main() {
    auto chirp_msg = Message("tb22", "sat1", MessageType::OFFER, ServiceIdentifier::CONTROL, 23999);

    auto io_context = asio::io_context();
    auto broadcaster = BroadcastSend(io_context);

    auto bin_msg = chirp_msg.Assemble();
    broadcaster.SendBroadcast(bin_msg.data(), bin_msg.size());
}
