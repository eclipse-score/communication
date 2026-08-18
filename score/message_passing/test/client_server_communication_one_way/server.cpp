#include "score/message_passing/test/client_server_communication_one_way/common_resources.h"

#include "score/message_passing/server_factory.h"
#include "score/message_passing/server_types.h"

#include "score/string_manipulation/arguments/arguments.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc, const char** argv)
{
    char message_char{};
    bool expecting_message{};
    std::string server_id;

    if (argc > 3)
    {
        auto args = score::string_manipulation::GetArguments(argc, argv);
        message_char = args[1].at(0);
        expecting_message = static_cast<bool>(args[2].at(0));
        server_id = std::string{args.at(3)};
    }
    else
    {
        std::cout << "All three arguments need to be provided.\n"
                     "1. A message charachter that will be compared to the received charachter.\n"
                     "2. A boolean value (0 or 1) indicating whether the server is expecting a message or not.\n"
                     "3. A string identifier for the server instance."
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Hello from server!" << std::endl;

    const score::message_passing::ServerFactory::ServerConfig server_config{10, 1, 5};

    score::message_passing::ServerFactory factory;
    auto server_ptr = factory.Create(score::message_passing::test::kTestServiceProtocolConfig, server_config);

    std::cout << server_id << " created successfully!" << std::endl;

    using score::message_passing::ConnectCallback;

    std::atomic_bool wait_for_connection = false;
    auto connect_callback = [&wait_for_connection,
                             &server_id](score::message_passing::IServerConnection& /*connection*/) {
        std::cout << server_id << ": Client connected!" << std::endl;

        wait_for_connection = true;
        return nullptr;
    };

    bool stop_listening = false;
    int exit_code = EXIT_SUCCESS;

    struct ServerState
    {
        std::string server_id;
        bool expecting_message;
        char message_char;
        bool& stop_listening;
        int& exit_code;

    } state{server_id, expecting_message, message_char, stop_listening, exit_code};

    score::message_passing::MessageCallback message_callback =
        [&state](score::message_passing::IServerConnection& /*connection*/,
                 score::cpp::span<const std::uint8_t> message) {
            std::cout << state.server_id << ": Received message from client: ";
            for (const auto& byte : message)
            {
                std::cout << std::hex << static_cast<char>(byte) << " ";
            }

            std::cout << std::dec << std::endl;

            state.stop_listening = true;
            if (!state.expecting_message)
            {
                std::cout << state.server_id << ": unexpectedly received a message." << std::endl;
                state.exit_code = EXIT_FAILURE;
                return score::cpp::expected_blank<score::os::Error>{};
            }

            if (message[0] != static_cast<std::uint8_t>(state.message_char))
            {
                std::cout << state.server_id << ": Received a wrong message. Expected: " << state.message_char
                          << ", but got: " << static_cast<char>(message[0]) << std::endl;
                state.exit_code = EXIT_FAILURE;
            }
            return score::cpp::expected_blank<score::os::Error>{};
        };
    // score::message_passing::DisconnectCallback{};
    auto disconnect_callback = [&server_id](score::message_passing::IServerConnection& /*connection*/) {
        std::cout << server_id << ": disconnect Callback" << std::endl;
        return;
    };

    server_ptr->StartListening(std::move(connect_callback),
                               std::move(disconnect_callback),
                               std::move(message_callback),
                               score::message_passing::MessageCallback{});

    constexpr auto per_loop_wait_time = std::chrono::milliseconds(10U);
    constexpr auto long_wait = std::chrono::milliseconds(5000);
    constexpr auto short_wait = std::chrono::milliseconds(500);
    const auto wait_time = expecting_message ? long_wait : short_wait;

    auto now = []() {
        return std::chrono::high_resolution_clock::now();
    };

    auto not_exceeded_wait_time = [wait_time](std::chrono::high_resolution_clock::time_point start_time,
                                              std::chrono::high_resolution_clock::time_point now_time) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now_time - start_time) < wait_time;
    };

    for (auto start_time = now(), now_time = now(); not_exceeded_wait_time(start_time, now_time); now_time = now())
    {
        if (!wait_for_connection)
        {
            break;
        }

        std::cout << server_id << ": Waiting for client to connect..." << std::endl;
        std::this_thread::sleep_for(per_loop_wait_time);
    }

    for (auto start_time = now(), now_time = now(); not_exceeded_wait_time(start_time, now_time); now_time = now())
    {
        if (!stop_listening)
        {
            break;
        }
        std::cout << server_id << ": Waiting for Incoming message..." << std::endl;
        std::this_thread::sleep_for(per_loop_wait_time);
    }

    server_ptr->StopListening();
    std::cout << server_id << ": stopped listening..." << std::endl;

    return EXIT_SUCCESS;
}
