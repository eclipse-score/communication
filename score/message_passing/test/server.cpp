
#include "score/message_passing/test/common.h"

#include "score/message_passing/i_server_connection.h"
#include "score/message_passing/server_factory.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace score::message_passing::test;

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    g_running.store(false, std::memory_order_relaxed);
}

int main(int /*argc*/, char** /*argv*/)
{
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    const score::message_passing::ServiceProtocolConfig protocol_config{
        kServiceIdentifier, kMaxSendSize, kMaxReplySize, kMaxNotifySize};
    const score::message_passing::IServerFactory::ServerConfig server_config{10U, 0U, 0U};

    score::message_passing::ServerFactory server_factory;
    auto server = server_factory.Create(protocol_config, server_config);
    if (!server)
    {
        std::cerr << "Failed to create server for " << kServiceIdentifier << std::endl;
        return EXIT_FAILURE;
    }

    auto connect_callback = [](score::message_passing::IServerConnection& /*connection*/) -> void* {
        return nullptr;
    };

    auto disconnect_callback = [](score::message_passing::IServerConnection& /*connection*/) {
    };

    auto sent_callback = [](score::message_passing::IServerConnection& /*connection*/,
                            score::cpp::span<const std::uint8_t> /*message*/)
        -> score::cpp::expected_blank<score::os::Error> {
        return {};
    };

    auto sent_with_reply_callback = [](score::message_passing::IServerConnection& connection,
                                       score::cpp::span<const std::uint8_t> message)
        -> score::cpp::expected_blank<score::os::Error> {
        const auto reply_result = connection.Reply(message);
        if (!reply_result.has_value())
        {
            std::cerr << "Failed to send reply: error code "
                      << reply_result.error().GetOsDependentErrorCode() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        return {};
    };

    const auto listen_result =
        server->StartListening(connect_callback, disconnect_callback, sent_callback, sent_with_reply_callback);
    if (!listen_result.has_value())
    {
        std::cerr << "Failed to start listening on " << kServiceIdentifier
                  << ": error code " << listen_result.error().GetOsDependentErrorCode() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Server is listening on " << kServiceIdentifier << std::endl;

    while (g_running.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server->StopListening();
    std::cout << "Server stopped." << std::endl;
    return EXIT_SUCCESS;
}

