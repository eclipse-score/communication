#include "score/message_passing/client_factory.h"
#include "score/message_passing/test/client_server_communication_one_way/common_resources.h"

#include "score/string_manipulation/arguments/arguments.h"

#include <score/span.hpp>

#include <unistd.h>
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
constexpr std::uint32_t kStateTryAttempts{10U};
constexpr std::chrono::milliseconds kStateRetryDelay{50};
}  // namespace

int main(int argc, const char** argv)
{
    char message_char{};
    if (argc > 1)
    {
        auto args = score::string_manipulation::GetArguments(argc, argv);
        message_char = args[1].at(0);
    }
    else
    {
        std::cout << "A message charachter needs to be provided. This is the charachter that will be transmitted."
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Hello from client!" << std::endl;
    score::message_passing::ClientFactory factory;
    const score::message_passing::IClientFactory::ClientConfig client_config{0U, 20U, false, true, false};

    auto client_ptr = factory.Create(score::message_passing::test::kTestServiceProtocolConfig, client_config);

    auto state_callback = [](score::message_passing::IClientConnection::State state) {
        std::cout << "Client state changed: " << static_cast<int>(state) << std::endl;
    };

    auto notify_callback = [](score::cpp::span<const std::uint8_t> message) {
        std::cout << "Received notify from server: " << std::endl;
        for (const auto& byte : message)
        {
            std::cout << std::hex << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << std::endl;
    };

    client_ptr->Start(state_callback, notify_callback);

    for (std::uint32_t try_attempt{0U}; try_attempt < kStateTryAttempts; ++try_attempt)
    {
        const auto state = client_ptr->GetState();
        if (state == score::message_passing::IClientConnection::State::kReady)
        {
            std::cout << "Client: Connection is ready!\n" << std::endl;
            break;
        }
        if (state != score::message_passing::IClientConnection::State::kStarting)
        {

            std::cout << "Client: Connection for " << score::message_passing::test::service_identifier
                      << " has failed to create, the reason is "
                      << static_cast<std::uint32_t>(score::cpp::to_underlying(client_ptr->GetStopReason()))
                      << std::endl;

            return EXIT_FAILURE;
        }
        std::this_thread::sleep_for(kStateRetryDelay);
    }

    std::cout << "Client created successfully!" << std::endl;

    std::array<std::uint8_t, score::message_passing::test::kMaxSendSize> message_to_send{
        static_cast<unsigned char>(message_char)};

    client_ptr->Send(message_to_send);
    std::cout << "Client: Message sent!" << std::endl;

    return EXIT_SUCCESS;
}
