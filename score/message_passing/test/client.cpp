
#include "score/message_passing/test/common.h"

#include "score/message_passing/client_connection.h"
#include "score/message_passing/client_factory.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

using namespace score::message_passing::test;

constexpr std::uint32_t kStateTryAttempts{32U};

constexpr std::chrono::milliseconds kStateRetryDelay{50};
constexpr std::chrono::milliseconds kSendCycleDelay{20};

static std::atomic<bool> g_running{true};

static void SignalHandler(int /*signum*/)
{
    g_running.store(false, std::memory_order_relaxed);
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <number_of_threads>" << std::endl;
        return EXIT_FAILURE;
    }

    const auto num_threads = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10));
    if (num_threads == 0U)
    {
        std::cerr << "Number of threads must be greater than 0" << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    const score::message_passing::ServiceProtocolConfig protocol_config{
        kServiceIdentifier, kMaxSendSize, kMaxReplySize, kMaxNotifySize};
    const score::message_passing::IClientFactory::ClientConfig client_config{0U, 20U, false, true, false};
    score::message_passing::ClientFactory client_factory;
    auto client_connection = client_factory.Create(protocol_config, client_config);

    client_connection->Start(score::message_passing::IClientConnection::StateCallback{},
                             score::message_passing::IClientConnection::NotifyCallback{});
    for (std::uint32_t try_attempt{0U}; try_attempt < kStateTryAttempts; ++try_attempt)
    {
        const auto state = client_connection->GetState();
        if (state == score::message_passing::IClientConnection::State::kReady)
        {
            break;
        }
        if (state != score::message_passing::IClientConnection::State::kStarting)
        {
            std::cerr << "Connection for " << kServiceIdentifier << " has failed to create, the reason is "
                      << static_cast<std::uint32_t>(score::cpp::to_underlying(client_connection->GetStopReason()))
                      << std::endl;
            return EXIT_FAILURE;
        }
        std::this_thread::sleep_for(kStateRetryDelay);
    }

    std::cout << "Client connection is ready, sending messages with " << num_threads << " threads..." << std::endl;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (std::uint32_t thread_id = 0U; thread_id < num_threads; ++thread_id)
    {
        threads.emplace_back([&client_connection, thread_id]() {
            while (g_running.load(std::memory_order_relaxed))
            {
                std::array<std::uint8_t, kMaxSendSize> send_buffer{};
                std::array<std::uint8_t, kMaxReplySize> reply_buffer{};

                // Encode the thread-unique ID into the send buffer
                std::memcpy(send_buffer.data(), &thread_id, sizeof(thread_id));

                auto result = client_connection->SendWaitReply(send_buffer, reply_buffer);
                if (!result.has_value())
                {
                    std::cerr << "Thread " << thread_id << ": SendWaitReply failed with error code "
                              << result.error().GetOsDependentErrorCode() << std::endl;
                    std::exit(EXIT_FAILURE);
                }

                const auto reply_span = result.value();
                if (reply_span.size() < sizeof(thread_id))
                {
                    std::cerr << "Thread " << thread_id << ": Reply too short (expected at least "
                              << sizeof(thread_id) << " bytes, got " << reply_span.size() << ")" << std::endl;
                    std::exit(EXIT_FAILURE);
                }

                std::uint32_t reply_id{};
                std::memcpy(&reply_id, reply_span.data(), sizeof(reply_id));
                if (reply_id != thread_id)
                {
                    std::cerr << "Thread " << thread_id << ": Reply mismatch! Expected " << thread_id
                              << " but got " << reply_id << std::endl;
                    std::exit(EXIT_FAILURE);
                }

                std::this_thread::sleep_for(kSendCycleDelay);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    client_connection->Stop();
    std::cout << "All threads stopped. Exiting." << std::endl;
    return EXIT_SUCCESS;
}
