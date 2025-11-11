/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include <gtest/gtest.h>

#include "score/message_passing/unix_domain/unix_domain_client_factory.h"
#include "score/message_passing/unix_domain/unix_domain_server_factory.h"

#include "score/message_passing/i_server_connection.h"

#include <future>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

void stdout_handler(const score::cpp::handler_parameters& param)
{
    std::cout << "In " << param.file << ":" << param.line << " " << param.function << " condition " << param.condition
              << " >> " << param.message << std::endl;
}

std::chrono::seconds kFutureWaitTimeout{5};

// param:
// - false: client and server use different engines with different background threads
// - true: client and server use share the engine and the background thread
class ServerToClientTestFixtureUnix : public ::testing::Test, public testing::WithParamInterface<bool>
{
  public:
    void SetUp() override
    {
        score::cpp::set_assertion_handler(stdout_handler);

        std::string test_prefix{"test_prefix_"};
        test_prefix += std::to_string(::getpid()) + "_";
        service_identifier_ = test_prefix + "1";
        protocol_config_ = ServiceProtocolConfig{service_identifier_, 1024, 1024, 1024};
        client_config_ = IClientFactory::ClientConfig{1, 1, false, true};

        server_connections_started_ = 0;
        server_connections_finished_ = 0;

        SetupClientPromises();
    }

    void TearDown() override
    {
        client_.reset();
        if (server_)
        {
            server_->StopListening();
            EXPECT_EQ(server_connections_finished_, server_connections_started_);
            server_.reset();
        }
    }

  protected:
    struct Promises
    {
        std::promise<void> ready;
        std::promise<void> stopping;
        std::promise<void> stopped;
    };

    struct Futures
    {
        std::future<void> ready;
        std::future<void> stopping;
        std::future<void> stopped;
    };

    void SetupClientPromises()
    {
        promises_ = Promises{};
        futures_ =
            Futures{promises_.ready.get_future(), promises_.stopping.get_future(), promises_.stopped.get_future()};
    }

    void WhenServerAndClientFactoriesConstructed(bool server_first = true, bool same_engine = true)
    {
        if (server_first)
        {
            server_factory_.emplace();
            if (same_engine)
            {
                client_factory_.emplace(server_factory_->GetEngine());
            }
            else
            {
                client_factory_.emplace();
            }
        }
        else
        {
            client_factory_.emplace();
            if (same_engine)
            {
                server_factory_.emplace(client_factory_->GetEngine());
            }
            else
            {
                server_factory_.emplace();
            }
        }
    }

    void WhenServerCreated()
    {
        server_ = server_factory_->Create(protocol_config_, server_config_);
        ASSERT_TRUE(server_);
    }

    void WhenRefusingServerStartsListening()
    {
        auto connect_callback = [](IServerConnection&) {
            std::cout << "RefusingConnectCallback" << std::endl;
            return score::cpp::make_unexpected(score::os::Error::createUnspecifiedError());
        };
        EXPECT_TRUE(server_->StartListening(connect_callback).has_value());
    }

    void WhenEchoServerStartsListening()
    {
        auto connect_callback = [this](IServerConnection& connection) -> void* {
            std::cout << "EchoConnectCallback " << &connection << std::endl;
            ++server_connections_started_;
            return nullptr;
        };
        auto disconnect_callback = [this](IServerConnection& connection) {
            std::cout << "EchoDisconnectCallback " << &connection << std::endl;
            ++server_connections_finished_;
        };
        auto sent_callback = [](IServerConnection& connection,
                                score::cpp::span<const std::uint8_t> message) -> score::cpp::blank {
            std::cout << "EchoSentCallback " << &connection << std::endl;
            connection.Notify(message);
            return {};
        };
        auto sent_with_reply_callback = [](IServerConnection& connection,
                                           score::cpp::span<const std::uint8_t> message) -> score::cpp::blank {
            std::cout << "EchoSentWithReplyCallback " << &connection << std::endl;
            connection.Reply(message);
            return {};
        };
        EXPECT_TRUE(
            server_->StartListening(connect_callback, disconnect_callback, sent_callback, sent_with_reply_callback)
                .has_value());
    }

    void WhenClientStarted(bool delete_on_stop = false)
    {
        delete_on_stop_ = delete_on_stop;
        client_ = client_factory_->Create(protocol_config_, client_config_);
        ASSERT_TRUE(client_);
        const auto state_callback = [this](auto state) {
            std::cout << "StateCallback " << static_cast<std::int32_t>(state) << std::endl;
            if (state == IClientConnection::State::kReady)
            {
                std::cout << "StateCallback Ready " << std::endl;
                promises_.ready.set_value();
            }
            else if (state == IClientConnection::State::kStopping)
            {
                std::cout << "StateCallback Stopping " << static_cast<std::int32_t>(client_->GetStopReason())
                          << std::endl;
                promises_.stopping.set_value();
            }
            else if (state == IClientConnection::State::kStopped)
            {
                std::cout << "StateCallback Stopped " << static_cast<std::int32_t>(client_->GetStopReason())
                          << std::endl;
                if (delete_on_stop_)
                {
                    std::lock_guard<std::mutex> guard(client_mutex_);
                    client_.reset();
                }
                promises_.stopped.set_value();
            }
        };

        std::lock_guard<std::mutex> guard(client_mutex_);
        client_->Start(state_callback, IClientConnection::NotifyCallback{});
    }

    void WhenClientStartedRestartingFromCallback(std::uint32_t retry_count)
    {
        retry_count_ = retry_count;
        client_ = client_factory_->Create(protocol_config_, client_config_);
        ASSERT_TRUE(client_);
        const auto state_callback = [this](auto state) {
            std::cout << "StateCallback " << static_cast<std::int32_t>(state) << std::endl;
            if (state == IClientConnection::State::kReady)
            {
                std::cout << "StateCallback Ready " << std::endl;
            }
            else if (state == IClientConnection::State::kStopping)
            {
                std::cout << "StateCallback Stopping " << static_cast<std::int32_t>(client_->GetStopReason())
                          << std::endl;
            }
            else if (state == IClientConnection::State::kStopped)
            {
                std::cout << "StateCallback Stopped " << static_cast<std::int32_t>(client_->GetStopReason())
                          << std::endl;
                if (retry_count_ > 0)
                {
                    --retry_count_;
                    client_->Restart();
                    return;
                }
                promises_.stopped.set_value();
            }
        };

        std::lock_guard<std::mutex> guard(client_mutex_);
        client_->Start(state_callback, IClientConnection::NotifyCallback{});
    }

    void WaitClientConnected()
    {
        ASSERT_EQ(futures_.ready.wait_for(kFutureWaitTimeout), std::future_status::ready);
    }

    void WaitClientStopping()
    {
        ASSERT_EQ(futures_.stopping.wait_for(kFutureWaitTimeout), std::future_status::ready);
    }

    void WaitClientStoppedExpectStatusStopped()
    {
        ASSERT_EQ(futures_.stopped.wait_for(kFutureWaitTimeout), std::future_status::ready);
        EXPECT_EQ(client_->GetState(), IClientConnection::State::kStopped);
    }

    void WaitClientStoppedExpectClientDeleted()
    {
        ASSERT_EQ(futures_.stopped.wait_for(kFutureWaitTimeout), std::future_status::ready);

        std::lock_guard<std::mutex> guard(client_mutex_);
        EXPECT_EQ(client_.get(), nullptr);
    }

    void WhenClientRestarted()
    {
        ASSERT_EQ(client_->GetState(), IClientConnection::State::kStopped);
        SetupClientPromises();
        client_->Restart();
    }

    void ExpectClientStillConnecting()
    {
        EXPECT_NE(futures_.ready.wait_for(std::chrono::milliseconds(10)), std::future_status::ready);

        std::lock_guard<std::mutex> guard(client_mutex_);
        EXPECT_EQ(client_->GetState(), IClientConnection::State::kStarting);
    }

    void WithStandardEchoServerSetup()
    {
        WhenServerAndClientFactoriesConstructed(false, GetParam());
        WhenClientStarted();

        ExpectClientStillConnecting();

        WhenServerCreated();
        WhenEchoServerStartsListening();

        WaitClientConnected();
    }

    void WhenClientSendsMessageItReceivesEchoReply()
    {
        std::array<std::uint8_t, 6> message = {1, 2, 3, 4, 5, 6};
        std::array<std::uint8_t, 256> reply_buffer = {};
        {
            std::promise<bool> promise;
            auto reply_callback =
                [&promise, &message](
                    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> message_expected) {
                    if (!message_expected.has_value())
                    {
                        promise.set_value(false);
                        return;
                    }
                    auto reply_message = message_expected.value();
                    promise.set_value(message.size() == static_cast<std::size_t>(reply_message.size()));
                };
            auto reply_expected = client_->SendWithCallback(message, reply_callback);
            ASSERT_TRUE(reply_expected.has_value());
            auto future = promise.get_future();
            ASSERT_EQ(future.wait_for(kFutureWaitTimeout), std::future_status::ready);
            EXPECT_TRUE(future.get());
        }
        {
            auto reply_expected = client_->SendWaitReply(message, reply_buffer);
            ASSERT_TRUE(reply_expected.has_value());
            auto reply = reply_expected.value();
            EXPECT_EQ(reply_buffer.data(), reply.data());
            EXPECT_EQ(message.size(), reply.size());
        }
    }

    Promises promises_;
    Futures futures_;

    IServerFactory::ServerConfig server_config_{};
    IClientFactory::ClientConfig client_config_{};
    std::optional<UnixDomainServerFactory> server_factory_;
    std::optional<UnixDomainClientFactory> client_factory_;

    score::cpp::pmr::unique_ptr<IServer> server_;
    std::mutex client_mutex_;
    score::cpp::pmr::unique_ptr<IClientConnection> client_;
    std::uint32_t server_connections_started_;
    std::uint32_t server_connections_finished_;

    std::string service_identifier_;
    ServiceProtocolConfig protocol_config_;

    bool delete_on_stop_{false};
    std::uint32_t retry_count_{0};
};

TEST_P(ServerToClientTestFixtureUnix, RefusingServerStartingFirst)
{
    WhenServerAndClientFactoriesConstructed(true, GetParam());
    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WhenClientStarted();

    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientTestFixtureUnix, RefusingServerStartingLater)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStarted();

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientTestFixtureUnix, RefusingServerStartingLaterClientDeleted)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStarted(true);

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WaitClientStoppedExpectClientDeleted();
}

TEST_P(ServerToClientTestFixtureUnix, RefusingServerStartingLaterClientRestarting)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStartedRestartingFromCallback(3);

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WaitClientStoppedExpectStatusStopped();
    EXPECT_EQ(retry_count_, 0);
}

TEST_P(ServerToClientTestFixtureUnix, EchoServerStartingLaterForcedStop)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStarted();

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenEchoServerStartsListening();

    WaitClientConnected();

    client_->Stop();
    WaitClientStopping();
    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientTestFixtureUnix, EchoServerSetup)
{
    WithStandardEchoServerSetup();

    WhenClientSendsMessageItReceivesEchoReply();

    client_->Stop();
    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientTestFixtureUnix, EchoServerClientRestart)
{
    WithStandardEchoServerSetup();

    WhenClientSendsMessageItReceivesEchoReply();

    client_->Stop();
    WaitClientStoppedExpectStatusStopped();

    WhenClientRestarted();
    WaitClientConnected();

    WhenClientSendsMessageItReceivesEchoReply();

    client_->Stop();
    WaitClientStoppedExpectStatusStopped();
}

INSTANTIATE_TEST_SUITE_P(UnixDomain, ServerToClientTestFixtureUnix, testing::Values(false, true));

}  // namespace
}  // namespace message_passing
}  // namespace score
