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

#include "score/message_passing/qnx_dispatch/qnx_dispatch_client_factory.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"

#include "score/message_passing/i_server_connection.h"

#include <future>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

void stderr_handler(const score::cpp::handler_parameters& param)
{
    std::cerr << "In " << param.file << ":" << param.line << " " << param.function << " condition " << param.condition
              << " >> " << param.message << std::endl;
}

std::chrono::seconds kFutureWaitTimeout{5};

// param:
// - false: client and server use different engines with different background threads
// - true: client and server use share the engine and the background thread
class ServerToClientQnxFixture : public ::testing::Test, public testing::WithParamInterface<bool>
{
  public:
    void SetUp() override
    {
        score::cpp::set_assertion_handler(stderr_handler);

        std::string test_prefix{"test_prefix_"};
        test_prefix += std::to_string(::getpid()) + "_";
        service_identifier_ = test_prefix + "1";
        protocol_config_ = ServiceProtocolConfig{service_identifier_, 6U, 6U, 6U};
        client_config_ = IClientFactory::ClientConfig{1U, 1U, false, true, false};
        server_config_ = IServerFactory::ServerConfig{0U, 0U, 1U};

        server_connections_started_ = 0U;
        server_connections_finished_ = 0U;

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
        ASSERT_TRUE(server_->StartListening(connect_callback).has_value());
    }

    void WhenEchoServerStartsListening()
    {
        auto connect_callback = [this](IServerConnection& connection) -> std::uintptr_t {
            std::cout << "EchoConnectCallback " << &connection << std::endl;
            ++server_connections_started_;
            const pid_t client_pid = connection.GetClientIdentity().pid;
            return static_cast<std::uintptr_t>(client_pid);
        };
        auto disconnect_callback = [this](IServerConnection& connection) {
            const auto client_pid = static_cast<pid_t>(std::get<std::uintptr_t>(connection.GetUserData()));

            std::cout << "EchoDisconnectCallback " << &connection << " " << client_pid << std::endl;
            ++server_connections_finished_;
        };
        auto sent_callback = [](IServerConnection& connection, score::cpp::span<const std::uint8_t> message) -> score::cpp::blank {
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
        ASSERT_TRUE(
            server_->StartListening(connect_callback, disconnect_callback, sent_callback, sent_with_reply_callback)
                .has_value());
    }

    void WhenFaultyEchoServerStartsListening()
    {
        class FaultyConnection : public IConnectionHandler
        {
          public:
            FaultyConnection(ServerToClientQnxFixture& fixture) : IConnectionHandler{}, fixture_{fixture} {}

          private:
            score::cpp::expected_blank<score::os::Error> OnMessageSent(IServerConnection& connection,
                                                              score::cpp::span<const std::uint8_t> message) noexcept override
            {
                std::cout << "FaultyEchoSentCallback " << &connection << std::endl;
                EXPECT_FALSE(connection.Notify(fixture_.faulty_message));  // fails: too big
                EXPECT_TRUE(connection.Notify(message));                   // OK
                EXPECT_FALSE(connection.Notify(message));                  // fails: slot taken
                return {};
            }
            score::cpp::expected_blank<score::os::Error> OnMessageSentWithReply(
                IServerConnection& connection,
                score::cpp::span<const std::uint8_t> message) noexcept override
            {
                std::cout << "FaultyEchoSentWithReplyCallback " << &connection << std::endl;
                EXPECT_FALSE(connection.Reply(fixture_.faulty_message));  // fails: too big
                EXPECT_TRUE(connection.Reply(message));                   // OK

                connection.RequestDisconnect();  // does nothing (yet), but we need to cover it
                return {};
            }

            void OnDisconnect(IServerConnection& connection) noexcept override
            {
                std::cout << "FaultyEchoDisconnectCallback " << &connection << std::endl;
                ++fixture_.server_connections_finished_;
            }

            ServerToClientQnxFixture& fixture_;
        };
        auto connect_callback = [this](IServerConnection& connection) -> score::cpp::pmr::unique_ptr<IConnectionHandler> {
            std::cout << "FaultyEchoConnectCallback " << &connection << std::endl;
            ++server_connections_started_;
            return score::cpp::pmr::make_unique<FaultyConnection>(score::cpp::pmr::get_default_resource(), *this);
        };
        ASSERT_TRUE(server_->StartListening(connect_callback, {}, {}, {}).has_value());
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

        const auto notify_callback = [this](auto message) {
            if (!client_notify_callback_.empty())
            {
                client_notify_callback_(message);
            }
        };

        std::lock_guard<std::mutex> guard(client_mutex_);
        client_->Start(state_callback, notify_callback);
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
                if (retry_count_ > 0U)
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

    void WithFaultyEchoServerSetup()
    {
        WhenServerAndClientFactoriesConstructed(false, GetParam());
        WhenClientStarted();

        ExpectClientStillConnecting();

        WhenServerCreated();
        WhenFaultyEchoServerStartsListening();

        WaitClientConnected();
    }

    void WhenClientSendsMessageItReceivesEchoReply()
    {
        std::array<std::uint8_t, 6U> message = {1U, 2U, 3U, 4U, 5U, 6U};
        std::array<std::uint8_t, 6U> reply_buffer = {};
        {
            std::promise<bool> promise;
            auto reply_callback =
                [&promise, &message](score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> message_expected) {
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
        {
            std::promise<bool> promise;
            client_notify_callback_ = [&promise, &message](score::cpp::span<const std::uint8_t> notify_message) {
                promise.set_value(message.size() == static_cast<std::size_t>(notify_message.size()));
            };
            auto send_expected = client_->Send(message);
            auto future = promise.get_future();
            ASSERT_EQ(future.wait_for(kFutureWaitTimeout), std::future_status::ready);
            EXPECT_TRUE(future.get());
        }
    }

    Promises promises_;
    Futures futures_;

    IServerFactory::ServerConfig server_config_{};
    IClientFactory::ClientConfig client_config_{};
    std::optional<QnxDispatchServerFactory> server_factory_;
    std::optional<QnxDispatchClientFactory> client_factory_;

    score::cpp::pmr::unique_ptr<IServer> server_;
    std::mutex client_mutex_;
    score::cpp::pmr::unique_ptr<IClientConnection> client_;
    std::uint32_t server_connections_started_;
    std::uint32_t server_connections_finished_;

    IClientConnection::NotifyCallback client_notify_callback_{};
    std::array<std::uint8_t, 8U> faulty_message = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};

    std::string service_identifier_;
    ServiceProtocolConfig protocol_config_;

    bool delete_on_stop_{false};
    std::uint32_t retry_count_{0U};
};

TEST_F(ServerToClientQnxFixture, ConstructServerThenClientFactoryUsingSameEngine)
{
    WhenServerAndClientFactoriesConstructed(true, true);

    EXPECT_EQ(server_factory_->GetEngine(), client_factory_->GetEngine());
}

TEST_F(ServerToClientQnxFixture, ConstructClientThenServerFactoryUsingSameEngine)
{
    WhenServerAndClientFactoriesConstructed(false, true);

    EXPECT_EQ(server_factory_->GetEngine(), client_factory_->GetEngine());
}

TEST_P(ServerToClientQnxFixture, RefusingServerStartingFirst)
{
    WhenServerAndClientFactoriesConstructed(true, GetParam());
    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WhenClientStarted();

    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientQnxFixture, RefusingServerStartingLater)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStarted();

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientQnxFixture, RefusingServerStartingLaterClientDeleted)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStarted(true);

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WaitClientStoppedExpectClientDeleted();
}

TEST_P(ServerToClientQnxFixture, RefusingServerStartingLaterClientRestarting)
{
    WhenServerAndClientFactoriesConstructed(false, GetParam());
    WhenClientStartedRestartingFromCallback(3);

    ExpectClientStillConnecting();

    WhenServerCreated();
    WhenRefusingServerStartsListening();

    WaitClientStoppedExpectStatusStopped();
    EXPECT_EQ(retry_count_, 0U);
}

TEST_P(ServerToClientQnxFixture, EchoServerStartingLaterForcedStop)
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

TEST_P(ServerToClientQnxFixture, EchoServerSetup)
{
    WithStandardEchoServerSetup();

    WhenClientSendsMessageItReceivesEchoReply();

    client_->Stop();
    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientQnxFixture, FaultyEchoServerSetup)
{
    WithFaultyEchoServerSetup();

    WhenClientSendsMessageItReceivesEchoReply();

    client_->Stop();
    WaitClientStoppedExpectStatusStopped();
}

TEST_P(ServerToClientQnxFixture, EchoServerClientRestart)
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

// "same engine" does not work for in-process client-server communications (_RESMGR_FLAG_SELF) on a single shared thread
INSTANTIATE_TEST_SUITE_P(QnxDispatch, ServerToClientQnxFixture, testing::Values(false /*, true*/));

}  // namespace
}  // namespace message_passing
}  // namespace score
