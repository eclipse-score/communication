/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/someip/service_discovery_client.h"

#include "score/message_passing/i_client_connection.h"
#include "score/message_passing/mock/client_connection_mock.h"
#include "score/message_passing/mock/client_factory_mock.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/someip/sd_client/sd_protocol.h"

#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace score::mw::com::impl::someip
{
namespace
{

namespace protocol = score::someip_sd::protocol;

using score::message_passing::ClientConnectionMock;
using score::message_passing::ClientConnectionMockFacade;
using score::message_passing::ClientFactoryMock;
using score::message_passing::IClientConnection;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

using SendResult = score::cpp::expected_blank<score::os::Error>;
using Reply = score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error>;

/// Only a hang guard: every wait below is expected to complete long before it.
constexpr auto kHangGuard = std::chrono::seconds{5};
/// Bounded observation window for asserting that a blocking call has not returned yet.
constexpr auto kNegativeWait = std::chrono::milliseconds{100};

constexpr std::uint16_t kService{0x1234U};
constexpr std::uint16_t kOtherService{0x2345U};
constexpr std::uint16_t kInstance{0x42U};

protocol::Message Availability(const std::uint16_t service, const std::uint16_t instance, const std::uint8_t flag)
{
    protocol::Message message{};
    message.opcode = protocol::Opcode::kAvailability;
    message.service = service;
    message.instance = instance;
    message.flag = flag;
    return message;
}

SendResult SendFailure()
{
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
}

/// FindServiceHandler has a small inline capacity; this forwards to a test-owned callable by reference.
template <typename Body>
FindServiceHandler<HandleType> ForwardTo(Body& body)
{
    return [&body](ServiceHandleContainer<HandleType> handles, FindServiceHandle handle) {
        body(std::move(handles), handle);
    };
}

/// One-shot event with a hang-guarded wait.
class Signal
{
  public:
    void Set()
    {
        {
            const std::lock_guard lock{mutex_};
            set_ = true;
        }
        condition_.notify_all();
    }

    bool WaitFor(const std::chrono::milliseconds timeout = kHangGuard)
    {
        std::unique_lock lock{mutex_};
        return condition_.wait_for(lock, timeout, [this]() {
            return set_;
        });
    }

    bool IsSet()
    {
        const std::lock_guard lock{mutex_};
        return set_;
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool set_{false};
};

/// Records the container sizes delivered to a FindServiceHandler (1 = available, 0 = unavailable).
class Recorder
{
  public:
    FindServiceHandler<HandleType> Handler(const FindServiceHandle expected_handle)
    {
        return [this, expected_handle](const ServiceHandleContainer<HandleType>& handles, FindServiceHandle handle) {
            EXPECT_EQ(handle, expected_handle);
            Record(handles);
        };
    }

    void Record(const ServiceHandleContainer<HandleType>& handles)
    {
        {
            const std::lock_guard lock{mutex_};
            sizes_.push_back(handles.size());
            for (const auto& handle : handles)
            {
                const auto instance_id = handle.GetInstanceId();
                const auto* const someip_instance_id = std::get_if<SomeIpServiceInstanceId>(&instance_id.binding_info_);
                instance_ids_.push_back(someip_instance_id == nullptr ? std::optional<std::uint16_t>{}
                                                                      : someip_instance_id->GetId());
            }
        }
        condition_.notify_all();
    }

    bool WaitForCount(const std::size_t count)
    {
        std::unique_lock lock{mutex_};
        return condition_.wait_for(lock, kHangGuard, [this, count]() {
            return sizes_.size() >= count;
        });
    }

    std::vector<std::size_t> Sizes()
    {
        const std::lock_guard lock{mutex_};
        return sizes_;
    }

    std::vector<std::optional<std::uint16_t>> InstanceIds()
    {
        const std::lock_guard lock{mutex_};
        return instance_ids_;
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<std::size_t> sizes_;
    std::vector<std::optional<std::uint16_t>> instance_ids_;
};

/// Single worker thread that plays the role of the message_passing IPC callback thread.
class IpcThread
{
  public:
    IpcThread()
        : thread_{[this]() {
              Run();
          }}
    {
    }

    ~IpcThread()
    {
        Stop();
    }

    IpcThread(const IpcThread&) = delete;
    IpcThread& operator=(const IpcThread&) = delete;
    IpcThread(IpcThread&&) = delete;
    IpcThread& operator=(IpcThread&&) = delete;

    void Post(std::function<void()> task)
    {
        {
            const std::lock_guard lock{mutex_};
            tasks_.push_back(std::move(task));
        }
        condition_.notify_all();
    }

    /// Runs all posted tasks, then joins.
    void Stop()
    {
        {
            const std::lock_guard lock{mutex_};
            stop_requested_ = true;
        }
        condition_.notify_all();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

  private:
    void Run()
    {
        std::unique_lock lock{mutex_};
        while (true)
        {
            condition_.wait(lock, [this]() {
                return stop_requested_ || !tasks_.empty();
            });
            if (tasks_.empty())
            {
                return;
            }
            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            lock.unlock();
            task();
            lock.lock();
        }
    }

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::function<void()>> tasks_;
    bool stop_requested_{false};
    std::thread thread_;
};

/// Drains and joins the IPC thread on scope exit, i.e. while the discovery client declared before it is still alive.
class ScopedIpcStop
{
  public:
    explicit ScopedIpcStop(IpcThread& ipc_thread) : ipc_thread_{ipc_thread} {}
    ~ScopedIpcStop()
    {
        ipc_thread_.Stop();
    }

    ScopedIpcStop(const ScopedIpcStop&) = delete;
    ScopedIpcStop& operator=(const ScopedIpcStop&) = delete;
    ScopedIpcStop(ScopedIpcStop&&) = delete;
    ScopedIpcStop& operator=(ScopedIpcStop&&) = delete;

  private:
    IpcThread& ipc_thread_;
};

/// Restores the default log recorder on scope exit.
class ScopedLogRecorder
{
  public:
    explicit ScopedLogRecorder(score::mw::log::Recorder& recorder)
    {
        score::mw::log::SetLogRecorder(&recorder);
    }
    ~ScopedLogRecorder()
    {
        score::mw::log::SetLogRecorder(nullptr);
    }

    ScopedLogRecorder(const ScopedLogRecorder&) = delete;
    ScopedLogRecorder& operator=(const ScopedLogRecorder&) = delete;
    ScopedLogRecorder(ScopedLogRecorder&&) = delete;
    ScopedLogRecorder& operator=(ScopedLogRecorder&&) = delete;
};

struct Connection
{
    ClientConnectionMock mock;
    IClientConnection::StateCallback state_callback;
    IClientConnection::NotifyCallback notify_callback;
    std::mutex sent_mutex;
    std::vector<protocol::Message> sent;

    score::cpp::pmr::unique_ptr<IClientConnection> MakeFacade()
    {
        return score::cpp::pmr::make_unique<NiceMock<ClientConnectionMockFacade>>(
            score::cpp::pmr::new_delete_resource(), mock);
    }

    void ExpectStartReady()
    {
        EXPECT_CALL(mock, Start(_, _)).WillOnce([this](auto state_cb, auto notify_cb) {
            state_callback = std::move(state_cb);
            notify_callback = std::move(notify_cb);
            state_callback(IClientConnection::State::kStarting);
            state_callback(IClientConnection::State::kReady);
        });
    }

    /// The connection becomes ready on the IPC thread only, as with the real single-threaded transport.
    void ExpectStartReadyOn(IpcThread& ipc_thread)
    {
        EXPECT_CALL(mock, Start(_, _)).WillOnce([this, &ipc_thread](auto state_cb, auto notify_cb) {
            state_callback = std::move(state_cb);
            notify_callback = std::move(notify_cb);
            ipc_thread.Post([this]() {
                state_callback(IClientConnection::State::kStarting);
                state_callback(IClientConnection::State::kReady);
            });
        });
    }

    void ExpectRegistration()
    {
        EXPECT_CALL(mock, SendWithCallback(_, _)).WillOnce([](auto, auto callback) {
            DeliverRegisterReply(callback);
            return SendResult{};
        });
    }

    /// The registration reply is delivered on the IPC thread only.
    void ExpectRegistrationOn(IpcThread& ipc_thread)
    {
        EXPECT_CALL(mock, SendWithCallback(_, _)).WillOnce([&ipc_thread](auto, auto callback) {
            auto reply_callback = std::make_shared<IClientConnection::ReplyCallback>(std::move(callback));
            ipc_thread.Post([reply_callback]() {
                DeliverRegisterReply(*reply_callback);
            });
            return SendResult{};
        });
    }

    void ExpectSynchronousStop()
    {
        EXPECT_CALL(mock, Stop()).WillOnce([this]() {
            Lose();
        });
    }

    void ExpectReadySession()
    {
        ExpectStartReady();
        ExpectRegistration();
        ExpectSynchronousStop();
    }

    void ExpectSends(const int times)
    {
        EXPECT_CALL(mock, Send(_)).Times(times).WillRepeatedly([this](auto data) {
            Record(data);
            return SendResult{};
        });
    }

    void Record(const score::cpp::span<const std::uint8_t> data)
    {
        const auto message = protocol::Decode(data.data(), data.size());
        EXPECT_TRUE(message.has_value());
        if (message.has_value())
        {
            const std::lock_guard lock{sent_mutex};
            sent.push_back(message.value());
        }
    }

    std::vector<protocol::Message> Sent()
    {
        const std::lock_guard lock{sent_mutex};
        return sent;
    }

    std::vector<protocol::Opcode> SentOpcodes()
    {
        std::vector<protocol::Opcode> opcodes{};
        for (const auto& message : Sent())
        {
            opcodes.push_back(message.opcode);
        }
        return opcodes;
    }

    void Notify(const protocol::Message& message) const
    {
        const auto buffer = protocol::Encode(message);
        notify_callback(score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()});
    }

    /// Unexpected loss as reported by the transport: kStopping followed by kStopped.
    void Lose() const
    {
        state_callback(IClientConnection::State::kStopping);
        state_callback(IClientConnection::State::kStopped);
    }

  private:
    static void DeliverRegisterReply(IClientConnection::ReplyCallback& callback)
    {
        protocol::Message response{};
        response.opcode = protocol::Opcode::kRegister;
        const auto buffer = protocol::Encode(response);
        callback(Reply{score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()}});
    }
};

/// Builds SOME/IP instance identifiers; the deployments are owned here because identifiers only reference them.
class SomeIpIdentifierFactory
{
  public:
    InstanceIdentifier Create(const std::uint16_t service,
                              const std::optional<std::uint16_t> instance,
                              const std::uint32_t major = 1U,
                              const std::uint32_t minor = 0U,
                              const QualityType quality_type = QualityType::kASIL_QM)
    {
        auto type_deployment = std::make_unique<ServiceTypeDeployment>(SomeIpServiceTypeDeployment{service});
        std::optional<SomeIpServiceInstanceId> instance_id{};
        if (instance.has_value())
        {
            instance_id = SomeIpServiceInstanceId{instance.value()};
        }
        auto instance_deployment = std::make_unique<ServiceInstanceDeployment>(
            make_ServiceIdentifierType("foo", major, minor),
            SomeIpServiceInstanceDeployment{instance_id},
            quality_type,
            InstanceSpecifier::Create(std::string{"/someip/discovery_test"}).value());
        const auto identifier = make_InstanceIdentifier(*instance_deployment, *type_deployment);
        type_deployments_.push_back(std::move(type_deployment));
        instance_deployments_.push_back(std::move(instance_deployment));
        return identifier;
    }

    EnrichedInstanceIdentifier Search(const std::uint16_t service,
                                      const std::uint32_t major = 1U,
                                      const std::uint32_t minor = 0U,
                                      const QualityType quality_type = QualityType::kASIL_QM)
    {
        return EnrichedInstanceIdentifier{Create(service, kInstance, major, minor, quality_type)};
    }

  private:
    std::vector<std::unique_ptr<ServiceTypeDeployment>> type_deployments_;
    std::vector<std::unique_ptr<ServiceInstanceDeployment>> instance_deployments_;
};

class ServiceDiscoveryClientTest : public ::testing::Test
{
  protected:
    void ExpectConnections(const std::vector<Connection*>& connections)
    {
        if (connections.empty())
        {
            EXPECT_CALL(factory_mock_, Create(_, _)).Times(0);
            return;
        }
        auto& expectation = EXPECT_CALL(factory_mock_, Create(_, _));
        for (auto* const connection : connections)
        {
            expectation.WillOnce([this, connection](auto, auto) {
                created_connections_.fetch_add(1U);
                return connection->MakeFacade();
            });
        }
    }

    Connection first_;
    Connection second_;
    ClientFactoryMock factory_mock_;
    std::atomic<std::size_t> created_connections_{0U};
    SomeIpIdentifierFactory identifiers_;
};

// Verifies: tests-1 — a nonzero service is offered and withdrawn (kBoth) with translated IDs and versions.
TEST_F(ServiceDiscoveryClientTest, OffersAndWithdrawsNonzeroServiceWithTranslatedFields)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    const auto identifier = identifiers_.Create(kService, kInstance, 3U, 7U);

    ASSERT_TRUE(client.OfferService(identifier).has_value());
    ASSERT_TRUE(client.StopOfferService(identifier, IServiceDiscovery::QualityTypeSelector::kBoth).has_value());

    const auto sent = first_.Sent();
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0].opcode, protocol::Opcode::kOfferService);
    EXPECT_EQ(sent[0].service, kService);
    EXPECT_EQ(sent[0].instance, kInstance);
    EXPECT_EQ(sent[0].major, 3U);
    EXPECT_EQ(sent[0].minor, 7U);
    EXPECT_EQ(sent[1].opcode, protocol::Opcode::kStopOfferService);
    EXPECT_EQ(sent[1].service, kService);
    EXPECT_EQ(sent[1].instance, kInstance);
    EXPECT_EQ(sent[1].major, 0U);
    EXPECT_EQ(sent[1].minor, 0U);
}

// Verifies: tests-9 — a quality-specific offer stop is rejected with the exact error and never connects.
TEST_F(ServiceDiscoveryClientTest, RejectsQualitySpecificOfferStopWithoutConnecting)
{
    ExpectConnections({});
    ServiceDiscoveryClient client{factory_mock_};

    const auto result = client.StopOfferService(identifiers_.Create(kService, kInstance),
                                                IServiceDiscovery::QualityTypeSelector::kAsilQm);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidBindingInformation);
}

// Verifies: tests-9 — identifiers without instance ID or with a major version above 255 never connect.
TEST_F(ServiceDiscoveryClientTest, RejectsMissingInstanceIdAndOversizedMajorWithoutConnecting)
{
    ExpectConnections({});
    ServiceDiscoveryClient client{factory_mock_};
    const std::vector<InstanceIdentifier> invalid_identifiers{identifiers_.Create(kService, std::nullopt),
                                                              identifiers_.Create(kService, kInstance, 256U, 0U)};
    Recorder recorder{};

    for (const auto& identifier : invalid_identifiers)
    {
        const auto offer = client.OfferService(identifier);
        ASSERT_FALSE(offer.has_value());
        EXPECT_EQ(offer.error(), ComErrc::kInvalidBindingInformation);

        const auto stop_offer = client.StopOfferService(identifier, IServiceDiscovery::QualityTypeSelector::kBoth);
        ASSERT_FALSE(stop_offer.has_value());
        EXPECT_EQ(stop_offer.error(), ComErrc::kInvalidBindingInformation);

        const auto handle = make_FindServiceHandle(1U);
        const auto search =
            client.StartFindService(handle, recorder.Handler(handle), EnrichedInstanceIdentifier{identifier});
        ASSERT_FALSE(search.has_value());
        EXPECT_EQ(search.error(), ComErrc::kInvalidBindingInformation);
        EXPECT_TRUE(client.StopFindService(handle).has_value());
    }
    EXPECT_TRUE(recorder.Sizes().empty());
}

// Verifies: tests-9 — major version 255 is the largest accepted value and is encoded unchanged.
TEST_F(ServiceDiscoveryClientTest, AcceptsMajorVersion255ForOfferAndSearch)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder recorder{};
    const auto handle = make_FindServiceHandle(3U);

    ASSERT_TRUE(client.OfferService(identifiers_.Create(kService, kInstance, 255U, 1U)).has_value());
    ASSERT_TRUE(client.StartFindService(handle, recorder.Handler(handle), identifiers_.Search(kOtherService, 255U, 2U))
                    .has_value());

    const auto sent = first_.Sent();
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0].opcode, protocol::Opcode::kOfferService);
    EXPECT_EQ(sent[0].major, 255U);
    EXPECT_EQ(sent[0].minor, 1U);
    EXPECT_EQ(sent[1].opcode, protocol::Opcode::kRequestService);
    EXPECT_EQ(sent[1].service, kOtherService);
    EXPECT_EQ(sent[1].major, 255U);
    EXPECT_EQ(sent[1].minor, 2U);
}

// Verifies: an ASIL-B-tagged identifier is not rejected because of its quality.
TEST_F(ServiceDiscoveryClientTest, StartsAndStopsFindServiceForAsilBIdentifier)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder recorder{};
    const auto handle = make_FindServiceHandle(9U);

    ASSERT_TRUE(client
                    .StartFindService(
                        handle, recorder.Handler(handle), identifiers_.Search(kService, 1U, 0U, QualityType::kASIL_B))
                    .has_value());
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    const auto sent = first_.Sent();
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0].opcode, protocol::Opcode::kRequestService);
    EXPECT_EQ(sent[0].service, kService);
    EXPECT_EQ(sent[0].instance, kInstance);
    EXPECT_EQ(sent[1].opcode, protocol::Opcode::kReleaseService);
}

// Verifies: tests-9 — a second search for the same instance with a different version is rejected without sending.
TEST_F(ServiceDiscoveryClientTest, RejectsConflictingVersionsForTheSameInstance)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder first_recorder{};
    Recorder conflicting_recorder{};
    const auto first_handle = make_FindServiceHandle(4U);
    const auto conflicting_handle = make_FindServiceHandle(5U);

    ASSERT_TRUE(
        client
            .StartFindService(first_handle, first_recorder.Handler(first_handle), identifiers_.Search(kService, 1U, 0U))
            .has_value());
    const auto conflicting = client.StartFindService(
        conflicting_handle, conflicting_recorder.Handler(conflicting_handle), identifiers_.Search(kService, 2U, 0U));
    ASSERT_FALSE(conflicting.has_value());
    EXPECT_EQ(conflicting.error(), ComErrc::kInvalidBindingInformation);
    EXPECT_TRUE(client.StopFindService(conflicting_handle).has_value());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(first_recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(first_handle).has_value());

    EXPECT_TRUE(conflicting_recorder.Sizes().empty());
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-4 — searches sharing one daemon request are notified on change only and released by the last owner.
TEST_F(ServiceDiscoveryClientTest, NotifiesAvailabilityAndReleasesLastSearch)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder first_recorder{};
    Recorder second_recorder{};
    const auto first_handle = make_FindServiceHandle(7U);
    const auto second_handle = make_FindServiceHandle(8U);
    ASSERT_TRUE(
        client.StartFindService(first_handle, first_recorder.Handler(first_handle), identifiers_.Search(kService))
            .has_value());
    ASSERT_TRUE(
        client.StartFindService(second_handle, second_recorder.Handler(second_handle), identifiers_.Search(kService))
            .has_value());

    // Flips of one search are awaited individually: queued flips are coalesced to the latest state (security-10).
    first_.Notify(Availability(kService, kInstance, 1U));
    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(first_recorder.WaitForCount(1U));
    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    first_.Notify(Availability(kService, kInstance, 0U));
    ASSERT_TRUE(first_recorder.WaitForCount(2U));
    ASSERT_TRUE(second_recorder.WaitForCount(2U));
    EXPECT_EQ(first_recorder.InstanceIds(), (std::vector<std::optional<std::uint16_t>>{kInstance}));

    ASSERT_TRUE(client.StopFindService(first_handle).has_value());
    EXPECT_EQ(first_.Sent().size(), 1U);

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(second_recorder.WaitForCount(3U));
    EXPECT_EQ(first_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_EQ(second_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U, 1U}));

    EXPECT_TRUE(client.StopFindService(first_handle).has_value());
    EXPECT_TRUE(client.StopFindService(second_handle).has_value());
    const auto sent = first_.Sent();
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0].opcode, protocol::Opcode::kRequestService);
    EXPECT_EQ(sent[0].service, kService);
    EXPECT_EQ(sent[0].instance, kInstance);
    EXPECT_EQ(sent[0].major, 1U);
    EXPECT_EQ(sent[0].minor, 0U);
    EXPECT_EQ(sent[1].opcode, protocol::Opcode::kReleaseService);
    EXPECT_EQ(sent[1].service, kService);
    EXPECT_EQ(sent[1].instance, kInstance);
}

// Verifies: safety-1 — availability arriving during registration does not deadlock and is reported after start.
TEST_F(ServiceDiscoveryClientTest, AvailabilityBeforeRegistrationReplyDoesNotDeadlockStart)
{
    ExpectConnections({&first_});
    first_.ExpectStartReady();
    first_.ExpectSynchronousStop();
    EXPECT_CALL(first_.mock, SendWithCallback(_, _)).WillOnce([this](auto, auto callback) {
        first_.Notify(Availability(kService, kInstance, 1U));
        protocol::Message response{};
        response.opcode = protocol::Opcode::kRegister;
        const auto buffer = protocol::Encode(response);
        callback(Reply{score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()}});
        return SendResult{};
    });
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder recorder{};
    const auto handle = make_FindServiceHandle(6U);

    ASSERT_TRUE(client.StartFindService(handle, recorder.Handler(handle), identifiers_.Search(kService)).has_value());
    ASSERT_TRUE(recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    EXPECT_EQ(recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-8 — the registration reply is delivered on the IPC thread only after the availability
// notification callback returned; start must neither deadlock nor lose the notification.
TEST_F(ServiceDiscoveryClientTest, RegistrationReplyAfterNotificationOnIpcThreadDoesNotDeadlock)
{
    IpcThread ipc_thread{};
    Signal notification_returned{};
    ExpectConnections({&first_});
    first_.ExpectStartReadyOn(ipc_thread);
    first_.ExpectSynchronousStop();
    EXPECT_CALL(first_.mock, SendWithCallback(_, _))
        .WillOnce([this, &ipc_thread, &notification_returned](auto, auto callback) {
            auto reply_callback = std::make_shared<IClientConnection::ReplyCallback>(std::move(callback));
            ipc_thread.Post([this, &notification_returned]() {
                first_.Notify(Availability(kService, kInstance, 1U));
                notification_returned.Set();
            });
            ipc_thread.Post([reply_callback, &notification_returned]() {
                EXPECT_TRUE(notification_returned.IsSet());
                protocol::Message response{};
                response.opcode = protocol::Opcode::kRegister;
                const auto buffer = protocol::Encode(response);
                (*reply_callback)(Reply{score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()}});
            });
            return SendResult{};
        });
    first_.ExpectSends(2);
    Recorder recorder{};
    ServiceDiscoveryClient client{factory_mock_};
    const ScopedIpcStop ipc_stop{ipc_thread};
    const auto handle = make_FindServiceHandle(9U);

    ASSERT_TRUE(client.StartFindService(handle, recorder.Handler(handle), identifiers_.Search(kService)).has_value());
    ASSERT_TRUE(recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    EXPECT_TRUE(notification_returned.IsSet());
    EXPECT_EQ(recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-2 — after a failed request send the SAME handle can be retried; the failed handler stays silent.
TEST_F(ServiceDiscoveryClientTest, FailedRequestSendRollsBackAndSameHandleCanRetry)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    EXPECT_CALL(first_.mock, Send(_))
        .Times(3)
        .WillOnce([this](auto data) {
            first_.Record(data);
            return SendFailure();
        })
        .WillRepeatedly([this](auto data) {
            first_.Record(data);
            return SendResult{};
        });
    ServiceDiscoveryClient client{factory_mock_};
    Recorder failed_recorder{};
    Recorder retry_recorder{};
    const auto handle = make_FindServiceHandle(11U);

    const auto failed = client.StartFindService(handle, failed_recorder.Handler(handle), identifiers_.Search(kService));
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), ComErrc::kBindingFailure);
    ASSERT_TRUE(
        client.StartFindService(handle, retry_recorder.Handler(handle), identifiers_.Search(kService)).has_value());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(retry_recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    EXPECT_TRUE(failed_recorder.Sizes().empty());
    EXPECT_EQ(retry_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(
        first_.SentOpcodes(),
        (std::vector<protocol::Opcode>{
            protocol::Opcode::kRequestService, protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: security-8 — a failed last release is logged and reported, and the local bookkeeping is still removed.
TEST_F(ServiceDiscoveryClientTest, FailedLastReleaseDropsBookkeepingAndReportsError)
{
    NiceMock<score::mw::log::RecorderMock> log_recorder{};
    EXPECT_CALL(log_recorder, StartRecord(_, _)).Times(AnyNumber());
    EXPECT_CALL(log_recorder, StartRecord(_, score::mw::log::LogLevel::kError)).Times(1);
    const ScopedLogRecorder log_scope{log_recorder};
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    EXPECT_CALL(first_.mock, Send(_)).Times(3).WillRepeatedly([this](auto data) {
        first_.Record(data);
        return (first_.Sent().size() == 2U) ? SendFailure() : SendResult{};
    });
    {
        ServiceDiscoveryClient client{factory_mock_};
        Recorder recorder{};
        const auto handle = make_FindServiceHandle(15U);
        ASSERT_TRUE(
            client.StartFindService(handle, recorder.Handler(handle), identifiers_.Search(kService)).has_value());

        const auto failed_stop = client.StopFindService(handle);
        ASSERT_FALSE(failed_stop.has_value());
        EXPECT_EQ(failed_stop.error(), ComErrc::kBindingFailure);
        // The handle is gone: a second stop neither retries nor sends anything.
        EXPECT_TRUE(client.StopFindService(handle).has_value());
        // The service entry is gone as well: a new search sends a fresh request.
        ASSERT_TRUE(
            client.StartFindService(handle, recorder.Handler(handle), identifiers_.Search(kService)).has_value());
    }
    EXPECT_EQ(
        first_.SentOpcodes(),
        (std::vector<protocol::Opcode>{
            protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService, protocol::Opcode::kRequestService}));
}

// Verifies: tests-3, cpp-4 — a handler can reentrantly start a search and stop its own search on the dispatcher.
TEST_F(ServiceDiscoveryClientTest, ReentrantHandlerCanStartAndStopSearches)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder first_recorder{};
    Recorder second_recorder{};
    const auto first_handle = make_FindServiceHandle(21U);
    const auto second_handle = make_FindServiceHandle(22U);
    const auto second_search = identifiers_.Search(kService);
    std::optional<bool> reentrant_start_succeeded{};
    std::optional<bool> reentrant_stop_succeeded{};
    auto first_handler = [&](const ServiceHandleContainer<HandleType>& handles, FindServiceHandle handle) {
        EXPECT_EQ(handle, first_handle);
        if (!reentrant_start_succeeded.has_value() && !handles.empty())
        {
            reentrant_start_succeeded =
                client.StartFindService(second_handle, second_recorder.Handler(second_handle), second_search)
                    .has_value();
            reentrant_stop_succeeded = client.StopFindService(first_handle).has_value();
        }
        first_recorder.Record(handles);
    };
    ASSERT_TRUE(
        client.StartFindService(first_handle, ForwardTo(first_handler), identifiers_.Search(kService)).has_value());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(first_recorder.WaitForCount(1U));
    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_EQ(reentrant_start_succeeded, std::optional<bool>{true});
    EXPECT_EQ(reentrant_stop_succeeded, std::optional<bool>{true});
    EXPECT_EQ(first_.SentOpcodes(), (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService}));

    first_.Notify(Availability(kService, kInstance, 0U));
    ASSERT_TRUE(second_recorder.WaitForCount(2U));
    ASSERT_TRUE(client.StopFindService(second_handle).has_value());

    EXPECT_EQ(first_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(second_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: cpp-4, cpp-6 — an external StopFindService waits for the running handler of that handle, and no
// queued or later notification reaches the handler after StopFindService returned.
TEST_F(ServiceDiscoveryClientTest, ExternalStopWaitsForRunningHandlerAndSuppressesLaterCallbacks)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(4);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder blocked_recorder{};
    Recorder sentinel_recorder{};
    Signal handler_entered{};
    Signal release_handler{};
    std::atomic<bool> handler_finished{false};
    const auto blocked_handle = make_FindServiceHandle(51U);
    const auto sentinel_handle = make_FindServiceHandle(52U);
    auto blocking_handler = [&](const ServiceHandleContainer<HandleType>& handles, FindServiceHandle) {
        blocked_recorder.Record(handles);
        handler_entered.Set();
        EXPECT_TRUE(release_handler.WaitFor());
        handler_finished = true;
    };
    ASSERT_TRUE(client.StartFindService(blocked_handle, ForwardTo(blocking_handler), identifiers_.Search(kService))
                    .has_value());
    ASSERT_TRUE(client
                    .StartFindService(
                        sentinel_handle, sentinel_recorder.Handler(sentinel_handle), identifiers_.Search(kOtherService))
                    .has_value());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(handler_entered.WaitFor());
    // Queued behind the running handler; must be purged by the stop below.
    first_.Notify(Availability(kService, kInstance, 0U));

    Signal stop_returned{};
    std::optional<bool> stop_succeeded{};
    bool finished_when_stop_returned{false};
    std::thread stopper{[&]() {
        stop_succeeded = client.StopFindService(blocked_handle).has_value();
        finished_when_stop_returned = handler_finished;
        stop_returned.Set();
    }};
    EXPECT_FALSE(stop_returned.WaitFor(kNegativeWait));
    release_handler.Set();
    EXPECT_TRUE(stop_returned.WaitFor());
    stopper.join();

    EXPECT_EQ(stop_succeeded, std::optional<bool>{true});
    EXPECT_TRUE(finished_when_stop_returned);
    // FIFO: once the sentinel is notified, everything produced before it has been dispatched.
    first_.Notify(Availability(kService, kInstance, 1U));
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    ASSERT_TRUE(sentinel_recorder.WaitForCount(1U));
    EXPECT_EQ(blocked_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    ASSERT_TRUE(client.StopFindService(sentinel_handle).has_value());
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService,
                                             protocol::Opcode::kRequestService,
                                             protocol::Opcode::kReleaseService,
                                             protocol::Opcode::kReleaseService}));
}

// Verifies: security-10, cpp-6 — notifications queued behind a running handler are coalesced per search: after any
// number of flips only the latest availability is delivered, and FIFO order across searches is kept (the sentinel
// queued after the coalesced entry is dispatched after it).
TEST_F(ServiceDiscoveryClientTest, CoalescesTogglesQueuedBehindRunningHandlerToLatestState)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    Recorder recorder{};
    Recorder sentinel_recorder{};
    Signal handler_entered{};
    Signal release_handler{};
    const auto handle = make_FindServiceHandle(53U);
    const auto sentinel_handle = make_FindServiceHandle(54U);
    auto handler = [&](const ServiceHandleContainer<HandleType>& handles, FindServiceHandle) {
        recorder.Record(handles);
        if (!handler_entered.IsSet())
        {
            handler_entered.Set();
            EXPECT_TRUE(release_handler.WaitFor());
        }
    };
    // The client must be destroyed before the objects its handlers reference.
    ServiceDiscoveryClient client{factory_mock_};
    ASSERT_TRUE(client.StartFindService(handle, ForwardTo(handler), identifiers_.Search(kService)).has_value());
    ASSERT_TRUE(client
                    .StartFindService(
                        sentinel_handle, sentinel_recorder.Handler(sentinel_handle), identifiers_.Search(kOtherService))
                    .has_value());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(handler_entered.WaitFor());
    for (std::size_t flip{0U}; flip < 500U; ++flip)
    {
        first_.Notify(Availability(kService, kInstance, 0U));
        first_.Notify(Availability(kService, kInstance, 1U));
    }
    first_.Notify(Availability(kService, kInstance, 0U));
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    release_handler.Set();

    ASSERT_TRUE(sentinel_recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_EQ(sentinel_recorder.Sizes(), (std::vector<std::size_t>{1U}));
}

// Verifies: security-10, tests-14 — coalesced flips that end in the last delivered state produce no callback, and
// coalesced flips of a never-reported search produce no empty first callback.
TEST_F(ServiceDiscoveryClientTest, CoalescedTogglesEndingInDeliveredStateProduceNoCallback)
{
    constexpr std::uint16_t kThirdService{0x3456U};
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(3);
    Recorder recorder{};
    Recorder never_reported_recorder{};
    Recorder sentinel_recorder{};
    Signal handler_entered{};
    Signal release_handler{};
    const auto handle = make_FindServiceHandle(55U);
    const auto never_reported_handle = make_FindServiceHandle(56U);
    const auto sentinel_handle = make_FindServiceHandle(57U);
    auto handler = [&](const ServiceHandleContainer<HandleType>& handles, FindServiceHandle) {
        recorder.Record(handles);
        if (!handler_entered.IsSet())
        {
            handler_entered.Set();
            EXPECT_TRUE(release_handler.WaitFor());
        }
    };
    // The client must be destroyed before the objects its handlers reference.
    ServiceDiscoveryClient client{factory_mock_};
    ASSERT_TRUE(client.StartFindService(handle, ForwardTo(handler), identifiers_.Search(kService)).has_value());
    ASSERT_TRUE(client
                    .StartFindService(never_reported_handle,
                                      never_reported_recorder.Handler(never_reported_handle),
                                      identifiers_.Search(kThirdService))
                    .has_value());
    ASSERT_TRUE(client
                    .StartFindService(
                        sentinel_handle, sentinel_recorder.Handler(sentinel_handle), identifiers_.Search(kOtherService))
                    .has_value());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(handler_entered.WaitFor());
    for (std::size_t flip{0U}; flip < 500U; ++flip)
    {
        first_.Notify(Availability(kService, kInstance, 0U));
        first_.Notify(Availability(kService, kInstance, 1U));
        first_.Notify(Availability(kThirdService, kInstance, 1U));
        first_.Notify(Availability(kThirdService, kInstance, 0U));
    }
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    release_handler.Set();

    ASSERT_TRUE(sentinel_recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_TRUE(never_reported_recorder.Sizes().empty());

    first_.Notify(Availability(kThirdService, kInstance, 1U));
    ASSERT_TRUE(never_reported_recorder.WaitForCount(1U));
    EXPECT_EQ(never_reported_recorder.Sizes(), (std::vector<std::size_t>{1U}));
}

// Verifies: cpp-8, tests-10 — a search can be restarted from the loss handler although the new connection only
// becomes ready (and registered) on the IPC thread that reported the loss.
TEST_F(ServiceDiscoveryClientTest, RestartFromLossHandlerSucceeds)
{
    IpcThread ipc_thread{};
    ExpectConnections({&first_, &second_});
    first_.ExpectStartReady();
    first_.ExpectRegistration();
    first_.ExpectSends(1);
    second_.ExpectStartReadyOn(ipc_thread);
    second_.ExpectRegistrationOn(ipc_thread);
    second_.ExpectSynchronousStop();
    second_.ExpectSends(2);
    Recorder old_recorder{};
    Recorder new_recorder{};
    Signal restart_done{};
    std::optional<bool> restart_succeeded{};
    const auto old_handle = make_FindServiceHandle(61U);
    const auto new_handle = make_FindServiceHandle(62U);
    const auto new_search = identifiers_.Search(kService);
    ServiceDiscoveryClient client{factory_mock_};
    const ScopedIpcStop ipc_stop{ipc_thread};
    auto old_handler = [&](const ServiceHandleContainer<HandleType>& handles, FindServiceHandle) {
        const bool lost = handles.empty();
        old_recorder.Record(handles);
        if (lost)
        {
            restart_succeeded =
                client.StartFindService(new_handle, new_recorder.Handler(new_handle), new_search).has_value();
            restart_done.Set();
        }
    };
    ASSERT_TRUE(client.StartFindService(old_handle, ForwardTo(old_handler), identifiers_.Search(kService)).has_value());

    ipc_thread.Post([this]() {
        first_.Notify(Availability(kService, kInstance, 1U));
    });
    ASSERT_TRUE(old_recorder.WaitForCount(1U));
    ipc_thread.Post([this]() {
        first_.Lose();
    });
    ASSERT_TRUE(restart_done.WaitFor());
    EXPECT_EQ(restart_succeeded, std::optional<bool>{true});

    ipc_thread.Post([this]() {
        second_.Notify(Availability(kService, kInstance, 1U));
    });
    ASSERT_TRUE(new_recorder.WaitForCount(1U));
    // The old handle was invalidated by the loss: stopping it must not release the new session's request.
    ASSERT_TRUE(client.StopFindService(old_handle).has_value());
    ASSERT_TRUE(client.StopFindService(new_handle).has_value());

    EXPECT_EQ(old_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_EQ(new_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(first_.SentOpcodes(), (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService}));
    EXPECT_EQ(second_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-10, correctness-3 — a loss is logged, notifies once, is not replayed, and the old handle cannot
// release the request of a manually restarted search in the new session.
TEST_F(ServiceDiscoveryClientTest, LossInvalidatesSearchWithoutReplayAndRestartUsesNewSession)
{
    NiceMock<score::mw::log::RecorderMock> log_recorder{};
    EXPECT_CALL(log_recorder, StartRecord(_, _)).Times(AnyNumber());
    EXPECT_CALL(log_recorder, StartRecord(_, score::mw::log::LogLevel::kError)).Times(1);
    const ScopedLogRecorder log_scope{log_recorder};
    ExpectConnections({&first_, &second_});
    first_.ExpectStartReady();
    first_.ExpectRegistration();
    first_.ExpectSends(1);
    second_.ExpectReadySession();
    second_.ExpectSends(2);
    Recorder old_recorder{};
    Recorder new_recorder{};
    const auto old_handle = make_FindServiceHandle(31U);
    const auto new_handle = make_FindServiceHandle(32U);
    {
        ServiceDiscoveryClient client{factory_mock_};
        ASSERT_TRUE(client.StartFindService(old_handle, old_recorder.Handler(old_handle), identifiers_.Search(kService))
                        .has_value());
        first_.Notify(Availability(kService, kInstance, 1U));
        ASSERT_TRUE(old_recorder.WaitForCount(1U));

        first_.Lose();
        ASSERT_TRUE(old_recorder.WaitForCount(2U));
        // No automatic replay: neither a new connection nor a request was made.
        EXPECT_EQ(created_connections_.load(), 1U);
        EXPECT_TRUE(second_.Sent().empty());
        ASSERT_TRUE(client.StopFindService(old_handle).has_value());

        ASSERT_TRUE(client.StartFindService(new_handle, new_recorder.Handler(new_handle), identifiers_.Search(kService))
                        .has_value());
        EXPECT_EQ(created_connections_.load(), 2U);
        second_.Notify(Availability(kService, kInstance, 1U));
        ASSERT_TRUE(new_recorder.WaitForCount(1U));
        ASSERT_TRUE(client.StopFindService(old_handle).has_value());
        EXPECT_EQ(second_.SentOpcodes(), (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService}));
        ASSERT_TRUE(client.StopFindService(new_handle).has_value());
    }
    EXPECT_EQ(old_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_EQ(new_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(first_.SentOpcodes(), (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService}));
    EXPECT_EQ(second_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: correctness-3, tests-17 — a search registered while the old session is being lost fails instead of
// succeeding silently, does not send its details to the replacement session, and earlier searches get exactly one
// loss notification.
TEST_F(ServiceDiscoveryClientTest, SearchStartedWhileSessionIsLostFailsAndDoesNotAffectReplacement)
{
    ExpectConnections({&first_, &second_});
    first_.ExpectReadySession();
    first_.ExpectSends(1);
    second_.ExpectReadySession();
    second_.ExpectSends(1);
    Recorder earlier_recorder{};
    Recorder lost_recorder{};
    Recorder replacement_recorder{};
    const auto earlier_handle = make_FindServiceHandle(40U);
    const auto lost_handle = make_FindServiceHandle(41U);
    const auto replacement_handle = make_FindServiceHandle(42U);
    // The client must be destroyed before the objects its handlers reference.
    ServiceDiscoveryClient client{factory_mock_};
    ASSERT_TRUE(client
                    .StartFindService(
                        earlier_handle, earlier_recorder.Handler(earlier_handle), identifiers_.Search(kOtherService))
                    .has_value());
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    ASSERT_TRUE(earlier_recorder.WaitForCount(1U));

    // The transport starts losing the session; kStopped follows when Connect() stops the connection.
    first_.state_callback(IClientConnection::State::kStopping);
    const auto lost =
        client.StartFindService(lost_handle, lost_recorder.Handler(lost_handle), identifiers_.Search(kService, 1U, 0U));
    ASSERT_FALSE(lost.has_value());
    EXPECT_EQ(lost.error(), ComErrc::kBindingFailure);
    EXPECT_TRUE(client.StopFindService(lost_handle).has_value());
    EXPECT_TRUE(second_.Sent().empty());

    ASSERT_TRUE(client
                    .StartFindService(replacement_handle,
                                      replacement_recorder.Handler(replacement_handle),
                                      identifiers_.Search(kService, 2U, 5U))
                    .has_value());
    second_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(replacement_recorder.WaitForCount(1U));

    EXPECT_EQ(earlier_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_TRUE(lost_recorder.Sizes().empty());
    EXPECT_EQ(replacement_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    const auto sent = second_.Sent();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(sent[0].opcode, protocol::Opcode::kRequestService);
    EXPECT_EQ(sent[0].service, kService);
    EXPECT_EQ(sent[0].major, 2U);
    EXPECT_EQ(sent[0].minor, 5U);
}

// Verifies: tests-17 — a loss injected while another control operation is in progress on the connection is
// reported exactly once to a search previously reported available.
TEST_F(ServiceDiscoveryClientTest, LossDuringControlOperationIsReportedExactlyOnce)
{
    ExpectConnections({&first_, &second_});
    first_.ExpectStartReady();
    first_.ExpectRegistration();
    EXPECT_CALL(first_.mock, Send(_))
        .Times(2)
        .WillOnce([this](auto data) {
            first_.Record(data);
            return SendResult{};
        })
        .WillOnce([this](auto data) {
            first_.Record(data);
            first_.Lose();
            return SendFailure();
        });
    second_.ExpectReadySession();
    second_.ExpectSends(1);
    Recorder lost_recorder{};
    Recorder sentinel_recorder{};
    const auto lost_handle = make_FindServiceHandle(71U);
    const auto sentinel_handle = make_FindServiceHandle(72U);
    // The client must be destroyed before the objects its handlers reference.
    ServiceDiscoveryClient client{factory_mock_};
    ASSERT_TRUE(client.StartFindService(lost_handle, lost_recorder.Handler(lost_handle), identifiers_.Search(kService))
                    .has_value());
    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(lost_recorder.WaitForCount(1U));

    const auto offer = client.OfferService(identifiers_.Create(kOtherService, kInstance));
    ASSERT_FALSE(offer.has_value());
    EXPECT_EQ(offer.error(), ComErrc::kBindingFailure);

    ASSERT_TRUE(client
                    .StartFindService(
                        sentinel_handle, sentinel_recorder.Handler(sentinel_handle), identifiers_.Search(kOtherService))
                    .has_value());
    second_.Notify(Availability(kOtherService, kInstance, 1U));
    ASSERT_TRUE(sentinel_recorder.WaitForCount(1U));
    EXPECT_EQ(lost_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
}

// Verifies: tests-14 — a search that stays unavailable while another service toggles gets a non-empty first callback.
TEST_F(ServiceDiscoveryClientTest, UnavailableSearchGetsNonEmptyFirstCallbackWhileOtherServiceToggles)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    Recorder waiting_recorder{};
    Recorder toggling_recorder{};
    const auto waiting_handle = make_FindServiceHandle(81U);
    const auto toggling_handle = make_FindServiceHandle(82U);
    // The client must be destroyed before the objects its handlers reference.
    ServiceDiscoveryClient client{factory_mock_};
    ASSERT_TRUE(
        client.StartFindService(waiting_handle, waiting_recorder.Handler(waiting_handle), identifiers_.Search(kService))
            .has_value());
    ASSERT_TRUE(client
                    .StartFindService(
                        toggling_handle, toggling_recorder.Handler(toggling_handle), identifiers_.Search(kOtherService))
                    .has_value());

    // Flips of one search are awaited individually: queued flips are coalesced to the latest state (security-10).
    first_.Notify(Availability(kService, kInstance, 0U));
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    ASSERT_TRUE(toggling_recorder.WaitForCount(1U));
    first_.Notify(Availability(kOtherService, kInstance, 0U));
    ASSERT_TRUE(toggling_recorder.WaitForCount(2U));
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    ASSERT_TRUE(toggling_recorder.WaitForCount(3U));
    EXPECT_TRUE(waiting_recorder.Sizes().empty());

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(waiting_recorder.WaitForCount(1U));
    EXPECT_EQ(waiting_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(toggling_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U, 1U}));
}

// Verifies: tests-14 — a loss before the first availability produces no callback for that search.
TEST_F(ServiceDiscoveryClientTest, LossBeforeFirstAvailabilityProducesNoCallback)
{
    ExpectConnections({&first_});
    first_.ExpectStartReady();
    first_.ExpectRegistration();
    first_.ExpectSends(2);
    Recorder never_available_recorder{};
    Recorder available_recorder{};
    const auto never_available_handle = make_FindServiceHandle(83U);
    const auto available_handle = make_FindServiceHandle(84U);
    // The client must be destroyed before the objects its handlers reference.
    ServiceDiscoveryClient client{factory_mock_};
    ASSERT_TRUE(client
                    .StartFindService(never_available_handle,
                                      never_available_recorder.Handler(never_available_handle),
                                      identifiers_.Search(kService))
                    .has_value());
    ASSERT_TRUE(client
                    .StartFindService(available_handle,
                                      available_recorder.Handler(available_handle),
                                      identifiers_.Search(kOtherService))
                    .has_value());
    first_.Notify(Availability(kOtherService, kInstance, 1U));
    ASSERT_TRUE(available_recorder.WaitForCount(1U));

    first_.Lose();
    ASSERT_TRUE(available_recorder.WaitForCount(2U));

    EXPECT_EQ(available_recorder.Sizes(), (std::vector<std::size_t>{1U, 0U}));
    EXPECT_TRUE(never_available_recorder.Sizes().empty());
}

// Verifies: correctness-3, tests-18 — ordinary owner shutdown is not reported as an unexpected session loss: no
// loss is logged (a loss would log an error) and the handler sees no empty notification.
TEST_F(ServiceDiscoveryClientTest, OrdinaryShutdownDoesNotNotifySearchUnavailable)
{
    NiceMock<score::mw::log::RecorderMock> log_recorder{};
    EXPECT_CALL(log_recorder, StartRecord(_, _)).Times(AnyNumber());
    EXPECT_CALL(log_recorder, StartRecord(_, score::mw::log::LogLevel::kError)).Times(0);
    const ScopedLogRecorder log_scope{log_recorder};
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(1);
    Recorder recorder{};
    {
        ServiceDiscoveryClient client{factory_mock_};
        const auto handle = make_FindServiceHandle(91U);
        ASSERT_TRUE(
            client.StartFindService(handle, recorder.Handler(handle), identifiers_.Search(kService)).has_value());
        first_.Notify(Availability(kService, kInstance, 1U));
        ASSERT_TRUE(recorder.WaitForCount(1U));
    }

    EXPECT_EQ(recorder.Sizes(), (std::vector<std::size_t>{1U}));
}

// Verifies: tests-19 (a) — a loss during the request send that still reports success fails the start (second session
// check), the failed handler is never called and the next search sends a fresh request on the new session.
TEST_F(ServiceDiscoveryClientTest, LossDuringSuccessfulRequestSendFailsStartAndNextSearchSendsFreshRequest)
{
    ExpectConnections({&first_, &second_});
    first_.ExpectStartReady();
    first_.ExpectRegistration();
    EXPECT_CALL(first_.mock, Send(_)).WillOnce([this](auto data) {
        first_.Record(data);
        first_.Lose();
        return SendResult{};
    });
    second_.ExpectReadySession();
    second_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder failed_recorder{};
    Recorder retry_recorder{};
    const auto handle = make_FindServiceHandle(101U);

    const auto failed = client.StartFindService(handle, failed_recorder.Handler(handle), identifiers_.Search(kService));
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), ComErrc::kBindingFailure);

    ASSERT_TRUE(
        client.StartFindService(handle, retry_recorder.Handler(handle), identifiers_.Search(kService)).has_value());
    second_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(retry_recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    EXPECT_TRUE(failed_recorder.Sizes().empty());
    EXPECT_EQ(retry_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(first_.SentOpcodes(), (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService}));
    EXPECT_EQ(second_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-19 (b) — a failed Connect() in StartFindService rolls the registration back, so the same handle and
// identifier can be started again.
TEST_F(ServiceDiscoveryClientTest, ConnectFailureInStartRollsBackAndSameHandleCanRetry)
{
    EXPECT_CALL(factory_mock_, Create(_, _))
        .WillOnce([](auto, auto) {
            return score::cpp::pmr::unique_ptr<IClientConnection>{};
        })
        .WillOnce([this](auto, auto) {
            return first_.MakeFacade();
        });
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder failed_recorder{};
    Recorder retry_recorder{};
    const auto handle = make_FindServiceHandle(102U);
    const auto search = identifiers_.Search(kService);

    const auto failed = client.StartFindService(handle, failed_recorder.Handler(handle), search);
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), ComErrc::kBindingFailure);

    ASSERT_TRUE(client.StartFindService(handle, retry_recorder.Handler(handle), search).has_value());
    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(retry_recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    EXPECT_TRUE(failed_recorder.Sizes().empty());
    EXPECT_EQ(retry_recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-19 (c) — a duplicate start with the same handle and identifier fails, sends nothing and does not
// duplicate notifications of the active search.
TEST_F(ServiceDiscoveryClientTest, DuplicateStartWithSameHandleAndIdentifierIsRejected)
{
    ExpectConnections({&first_});
    first_.ExpectReadySession();
    first_.ExpectSends(2);
    ServiceDiscoveryClient client{factory_mock_};
    Recorder recorder{};
    Recorder duplicate_recorder{};
    const auto handle = make_FindServiceHandle(103U);
    const auto search = identifiers_.Search(kService);

    ASSERT_TRUE(client.StartFindService(handle, recorder.Handler(handle), search).has_value());
    const auto duplicate = client.StartFindService(handle, duplicate_recorder.Handler(handle), search);
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error(), ComErrc::kBindingFailure);

    first_.Notify(Availability(kService, kInstance, 1U));
    ASSERT_TRUE(recorder.WaitForCount(1U));
    ASSERT_TRUE(client.StopFindService(handle).has_value());

    EXPECT_EQ(recorder.Sizes(), (std::vector<std::size_t>{1U}));
    EXPECT_TRUE(duplicate_recorder.Sizes().empty());
    EXPECT_EQ(first_.SentOpcodes(),
              (std::vector<protocol::Opcode>{protocol::Opcode::kRequestService, protocol::Opcode::kReleaseService}));
}

// Verifies: tests-19 (d) — Connect() failures in OfferService/StopOfferService and a failed stop-offer send are
// reported as kBindingFailure.
TEST_F(ServiceDiscoveryClientTest, OfferConnectAndStopOfferSendFailuresReportBindingFailure)
{
    EXPECT_CALL(factory_mock_, Create(_, _))
        .WillOnce([](auto, auto) {
            return score::cpp::pmr::unique_ptr<IClientConnection>{};
        })
        .WillOnce([](auto, auto) {
            return score::cpp::pmr::unique_ptr<IClientConnection>{};
        })
        .WillOnce([this](auto, auto) {
            return first_.MakeFacade();
        });
    first_.ExpectReadySession();
    EXPECT_CALL(first_.mock, Send(_)).WillOnce([this](auto data) {
        first_.Record(data);
        return SendFailure();
    });
    ServiceDiscoveryClient client{factory_mock_};
    const auto identifier = identifiers_.Create(kService, kInstance);

    const auto offer = client.OfferService(identifier);
    ASSERT_FALSE(offer.has_value());
    EXPECT_EQ(offer.error(), ComErrc::kBindingFailure);

    const auto stop_offer_connect = client.StopOfferService(identifier, IServiceDiscovery::QualityTypeSelector::kBoth);
    ASSERT_FALSE(stop_offer_connect.has_value());
    EXPECT_EQ(stop_offer_connect.error(), ComErrc::kBindingFailure);

    const auto stop_offer_send = client.StopOfferService(identifier, IServiceDiscovery::QualityTypeSelector::kBoth);
    ASSERT_FALSE(stop_offer_send.has_value());
    EXPECT_EQ(stop_offer_send.error(), ComErrc::kBindingFailure);
    EXPECT_EQ(first_.SentOpcodes(), (std::vector<protocol::Opcode>{protocol::Opcode::kStopOfferService}));
}

// Verifies: tests-19 (e) — synchronous FindService is unsupported and never connects.
TEST_F(ServiceDiscoveryClientTest, FindServiceIsRejectedWithoutConnecting)
{
    ExpectConnections({});
    ServiceDiscoveryClient client{factory_mock_};

    const auto result = client.FindService(identifiers_.Search(kService));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kInvalidBindingInformation);
}

}  // namespace
}  // namespace score::mw::com::impl::someip
