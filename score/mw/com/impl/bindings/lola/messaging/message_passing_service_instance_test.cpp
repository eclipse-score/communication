#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"
#include "score/message_passing/mock/client_connection_mock.h"
#include "score/message_passing/mock/client_factory_mock.h"
#include "score/message_passing/mock/server_connection_mock.h"
#include "score/message_passing/mock/server_factory_mock.h"
#include "score/message_passing/server_types.h"
#include <gtest/gtest.h>

#include "score/concurrency/executor_mock.h"
#include "score/message_passing/mock/server_mock.h"

#include "score/os/mocklib/unistdmock.h"

namespace score::mw::com::impl::lola
{

class MessagePassingServiceInstanceAttorney
{
  public:
    using MessageType = MessagePassingServiceInstance::MessageType;
    static constexpr auto max_receive_handlers_per_event = MessagePassingServiceInstance::kMaxReceiveHandlersPerEvent;
    static constexpr auto node_id_tmp_buffer_size = MessagePassingServiceInstance::NodeIdTmpBufferSize;
};

namespace
{

using namespace score::message_passing;
using MessageType = MessagePassingServiceInstanceAttorney::MessageType;

class MessagePassingServiceInstanceTest : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(*server_mock_, StartListening(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([this](ConnectCallback cc,
                                  DisconnectCallback dc,
                                  MessageCallback message_received_cb,
                                  MessageCallback message_received_w_reply_cb) -> score::cpp::expected_blank<score::os::Error> {
                connect_callback_ = std::move(cc);
                disconnect_callback_ = std::move(dc);
                received_send_message_callback_ = std::move(message_received_cb);
                received_send_message_with_reply_callback_ = std::move(message_received_w_reply_cb);

                return {};
            });

        ON_CALL(server_factory_mock_, Create(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(::testing::ByMove(std::move(server_mock_))));

        auto client_connection_mock_facade = score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMockFacade>>(
            score::cpp::pmr::new_delete_resource(), client_connection_mock_);
        ON_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(::testing::ByMove(std::move(client_connection_mock_facade))));

        ON_CALL(client_connection_mock_, GetState()).WillByDefault(testing::Return(IClientConnection::State::kReady));

        ON_CALL(*server_connection_mock_, GetClientIdentity()).WillByDefault(::testing::ReturnRef(client_identity_));
        ON_CALL(*server_connection_mock_, GetUserData()).WillByDefault(::testing::ReturnRef(user_data_));

        ON_CALL(*unistd_mock_, getpid()).WillByDefault(::testing::Return(local_pid_));

        ON_CALL(executor_mock_, Enqueue(testing::_)).WillByDefault([this](auto&& task) {
            executor_task_ = std::forward<decltype(task)>(task);
        });
    }

    template <typename T>
    score::cpp::span<const uint8_t> Serialize(const T& el_id, MessageType message_type, bool valid = true)
    {
        static std::vector<uint8_t> buffer{};
        if (valid)
        {
            buffer.assign(sizeof(T) + 1, 0);
            buffer[0] = score::cpp::to_underlying(message_type);
            memcpy(buffer.data() + 1, &el_id, sizeof(T));
        }
        else
        {
            // +5 is arbitrary, whatever's != (sizeof(T) + 1) works
            // we don't care about the content since it will be filtered before parsing
            buffer.assign(sizeof(T) + 5, 0);
            buffer[0] = score::cpp::to_underlying(message_type);
        }
        return {buffer.data(), buffer.size()};
    }

    ::testing::NiceMock<ClientFactoryMock> client_factory_mock_{};
    ::testing::NiceMock<ServerFactoryMock> server_factory_mock_{};

    score::cpp::pmr::unique_ptr<testing::NiceMock<ServerMock>> server_mock_{
        score::cpp::pmr::make_unique<testing::NiceMock<ServerMock>>(score::cpp::pmr::get_default_resource())};

    concurrency::testing::ExecutorMock executor_mock_{};
    score::cpp::pmr::unique_ptr<score::concurrency::Task> executor_task_{};
    score::cpp::stop_token stop_token_{};

    AsilSpecificCfg asil_cfg_{};
    ClientQualityType quality_type_{ClientQualityType::kASIL_B};

    ::testing::NiceMock<ClientConnectionMock> client_connection_mock_{};
    score::cpp::pmr::unique_ptr<::testing::NiceMock<ServerConnectionMock>> server_connection_mock_{
        score::cpp::pmr::make_unique<::testing::NiceMock<ServerConnectionMock>>(score::cpp::pmr::new_delete_resource())};

    ConnectCallback connect_callback_{};
    DisconnectCallback disconnect_callback_{};
    MessageCallback received_send_message_callback_{};
    MessageCallback received_send_message_with_reply_callback_{};

    score::safecpp::Scope<> scope_{};
    os::MockGuard<testing::NiceMock<os::UnistdMock>> unistd_mock_{};

    pid_t local_pid_{1};
    pid_t remote_pid_{2};

    ClientIdentity client_identity_{15u, 25u, 35u};
    UserData user_data_{std::in_place_type<uintptr_t>, 10u};

    ElementFqId event_id_;
};

using MessagePassingServiceInstanceDeathTest = MessagePassingServiceInstanceTest;

TEST_F(MessagePassingServiceInstanceDeathTest, TerminationOnStartListeningFail)
{
    // Given server mock that returns error on StartListening
    auto server = score::cpp::pmr::make_unique<ServerMock>(score::cpp::pmr::get_default_resource());
    ON_CALL(*server, StartListening(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // and server factory mock that returns the server mock
    ON_CALL(server_factory_mock_, Create(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(::testing::ByMove(std::move(server))));

    // Expect termination
    EXPECT_DEATH(
        {
            // When MessagePassingServiceInstance is constructed
            MessagePassingServiceInstance(
                quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_);
        },
        ".*");
}

// connect/disconnect
TEST_F(MessagePassingServiceInstanceTest, ConnectCallbackReturnsClientPid)
{
    // Given ServerConnection mock
    // and MessagePassingServiceInstance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When connect callback is invoked with the mock
    const auto pid_result = connect_callback_(*server_connection_mock_);

    // Expect return value contains user pid
    ASSERT_TRUE(pid_result.has_value());
    EXPECT_EQ(std::get<uintptr_t>(pid_result.value()), client_identity_.pid);
}

TEST_F(MessagePassingServiceInstanceTest, DisconnectCallbackSuccessfullyExecuted)
{
    // Just a placeholder due to disconnect callback being empty
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    disconnect_callback_(*server_connection_mock_);
}

// received_send_message
TEST_F(MessagePassingServiceInstanceTest, DoesNotTerminateUponReceivingAnEmptyMessage)
{
    // Given message passing instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When message callback is called with an empty message
    // Expect no termination
    received_send_message_callback_(*server_connection_mock_, {});
}

TEST_F(MessagePassingServiceInstanceTest, DoesNotTerminateUponReceivingMessageOfInvalidType)
{
    // Given message passing instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When message callback is called with a message of incorrect type
    // Expect no termination
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, static_cast<MessageType>(0)));
}

// incorrect length
TEST_F(MessagePassingServiceInstanceTest, DoesNotTerminateUponReceivingRegisterEventNotifierWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier, false));
}

TEST_F(MessagePassingServiceInstanceTest, DoesNotTerminateUponReceivingUnregisterEventNotifierWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier, false));
}

TEST_F(MessagePassingServiceInstanceTest, DoesNotTerminateUponReceivingNotifyEventWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent, false));
}

TEST_F(MessagePassingServiceInstanceTest, DoesNotTerminateUponReceivingOutdatedNodeIdWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(std::get<uintptr_t>(user_data_), MessageType::kOutdatedNodeId, false));
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventLocallyCallsRegisteredHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler to track whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and handler being registered for event
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When NotifyEvent is called for the same event
    instance.NotifyEvent(event_id_);
    (*executor_task_)(stop_token_);

    // Expect handler to be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventLocallyDoesNotTerminateUponEncounteringDestroyedHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an empty weak_ptr being registered for event
    instance.RegisterEventNotification(event_id_, {}, local_pid_);

    // Then we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);

    // Expect no termination of NotifyEventLocally
    (*executor_task_)(stop_token_);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventLocallyDoesNotCallUnregisteredHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and handler being registered for event
    auto handler_registration = instance.RegisterEventNotification(event_id_, {}, local_pid_);

    // and then unregistered
    instance.UnregisterEventNotification(event_id_, handler_registration, local_pid_);

    // When we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);

    // Then NotifyEventLocally shall not be enqueued
    EXPECT_EQ(executor_task_.get(), nullptr);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationCalledWithNonExistingHandlerRegistrationDoesNotAffectExistingRegistrations)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler with indication whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and handler being registered for event
    auto handler_registration = instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // and then UnregisterEventNotification is called with a different handle
    instance.UnregisterEventNotification(event_id_, ++handler_registration, local_pid_);

    // When we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);
    (*executor_task_)(stop_token_);

    // Then the handler has been called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventDoesNotPostNotifyEventLocallyIfNothingRegisteredForTheEvent)
{
    // Given service instance with no registered handlers
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When we call NotifyEvent for an event
    instance.NotifyEvent(event_id_);

    // Then NotifyEventLocally has not been enqueued.
    EXPECT_EQ(executor_task_.get(), nullptr);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventLocallyCallsNoMoreThanMaxPossibleHandlersPerEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and handler that increments number of calls
    uint16_t nums_called{0};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&nums_called]() {
            ++nums_called;
        });

    // and handler being registered for event (max_receive_handlers_per_event + 2) times
    for (auto i = 0; i < MessagePassingServiceInstanceAttorney::max_receive_handlers_per_event + 2; ++i)
    {
        instance.RegisterEventNotification(event_id_, handler, local_pid_);
    }

    // When we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);
    (*executor_task_)(stop_token_);

    // Then handler should be called max_receive_handlers_per_event times
    EXPECT_EQ(nums_called, MessagePassingServiceInstanceAttorney::max_receive_handlers_per_event);
}

TEST_F(MessagePassingServiceInstanceTest, RegisterEventNotificationRemoteSendsRegisterMessageOnFirstRegistration)
{
    RecordProperty("Verifies", "SCR-5899276");
    RecordProperty("Description", "Register Event notification callback.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // When handler being registered with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       RegisterEventNotificationRemoteDoesNotTerminateWhenFailsToSendRegistrationMessage)
{
    RecordProperty("Verifies", "SCR-5899276");
    RecordProperty("Description", "Register Event notification callback.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called and return an error
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // When handler being registered with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       RegisterEventNotificationRemoteSendsRegistrationMessageOnlyOnInitialRegistration)
{
    RecordProperty("Verifies", "SCR-5899276");
    RecordProperty("Description", "Register Event notification callback.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // When handler being registered with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       RegisterEventNotificationRemoteReplacesNodeIdWhenCalledWithDifferentForSameEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client factory mock to be called once for each pid
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Invoke([](auto&&...) {
            auto client_connection_mock =
                score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());
            EXPECT_CALL(*client_connection_mock, Send(::testing::_)).Times(1);

            return client_connection_mock;
        }));

    // When handler being registered for event with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    instance.RegisterEventNotification(event_id_, {}, remote_pid_ + 5);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationRemoteSendsUnregisterMessageUponRemovalOfLastRegistration)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration and once upon unregistration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // Given handler being registered with pid != local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationRemoteDoesNotTerminateUponUnregistrationMessageSendFailure)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called twice: on registration and unregistration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // Given handler being registered for event with pid != local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterEventNotificationRemoteDoesNotUnregisterOnPidMismatch)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // Given handler being registered with pid != local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_ + 5);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationRemoteDoesNotSendMessageUponUnregisteringNonLastHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // Given handler being registered with pid != local_pid twice
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterEventNotificationRemoteDoesNotUnregisterLocalRegistrations)  // TODO
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client factory to not be requested to create new connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // Given handler being registered with local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, local_pid_);

    // When UnregisterEventNotification is called for the same event but remote pid
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

// notify remote
TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteNotifiesClients)
{
    RecordProperty("Verifies", "SCR-5898962, SCR-5899250, SCR-5899276, SCR-5899282");
    RecordProperty("Description", "Remote receiver is notified via Message Passing.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // When NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteWontNotifyClientRegisteredForDifferentEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client factory mock to not be requested to create a new connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When NotifyEvent() for a different event is called
    ++event_id_.element_id_;
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteNotifiesClientsRegisteredTwice)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received twice
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // When NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteDoesNotTerminateOnSendFail)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // and client connection that returns an error on Send()
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // When NotifyEvent() for the same event is called
    // Expect it to not terminate
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteNotifiesClientsExceedingTmpNodeBuffer)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    uint16_t nums_called{0};
    auto size = MessagePassingServiceInstanceAttorney::node_id_tmp_buffer_size * 2;

    // and (MessagePassingServiceInstanceAttorney::node_id_tmp_buffer_size * 2) registered for the same event
    for (auto i = 0; i < size; ++i)
    {
        received_send_message_callback_(*server_connection_mock_,
                                        Serialize(event_id_, MessageType::kRegisterEventNotifier));
        user_data_.emplace<uintptr_t>(std::get<uintptr_t>(user_data_) + 1);
    }

    // and client factory mock that returns new connections
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .Times(size)
        .WillRepeatedly(::testing::Invoke([&nums_called](auto&&...) {
            auto mock =
                score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());

            EXPECT_CALL(*mock, Send(::testing::_)).WillOnce(testing::Invoke([&nums_called]() {
                ++nums_called;
                return score::cpp::expected_blank<score::os::Error>{};
            }));

            return mock;
        }));

    // When NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);

    // Then all clients should be notified
    EXPECT_EQ(nums_called, size);
}

// unregister message
TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteDoesNotNotifyUnregisteredClient)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client factory mock's Create() to not be called
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When unregister event notifier message is received for the same client
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));

    // Then NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterMessageForNonExistingRegistrationDoesNotAffectTheExistingRegistrations)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_)).Times(1);

    // When unregister event notifier message is received for different event
    ++event_id_.element_id_;
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));

    // Then NotifyEvent() for the initial event is called
    --event_id_.element_id_;
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, MultipleUnregistrationsForTheSameClientDoNotLeadToTermination)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client factory mock to not be requested to create a new client
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When unregister event notifier message is received for the same client twice
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));

    // Then NotifyEvent() for the same event is called
    // Expect no termination
    instance.NotifyEvent(event_id_);
}

// handle outdated msg
TEST_F(MessagePassingServiceInstanceTest, HandleOutdatedNodeIdMsgRemovesOutdatedPid)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect, that client factory mock is not requested to create a new client
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When pid which was registered for event marked as outdated
    received_send_message_callback_(
        *server_connection_mock_,
        Serialize(static_cast<pid_t>(std::get<uintptr_t>(user_data_)), MessageType::kOutdatedNodeId));

    // Then NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, HandleOutdatedNodeIdCalledWithUnregisteredPidDoesNotAffectOtherRegistrations)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_)).Times(1);

    // Given notification is registered
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // When kOutdatedNodeId message received for another pid
    received_send_message_callback_(
        *server_connection_mock_,
        Serialize(static_cast<pid_t>(std::get<uintptr_t>(user_data_) + 1), MessageType::kOutdatedNodeId));

    // Then NotifyEvent is called
    instance.NotifyEvent(event_id_);
}

// notify event message
TEST_F(MessagePassingServiceInstanceTest, NotifyEventMessageCallsRegisteredHandler)
{
    RecordProperty("Verifies", "SCR-5898962, SCR-5899250, SCR-5899276, SCR-5899282");
    RecordProperty("Description", "Registered callback for event-notification gets invoked");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler to track whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // when handler being registered for event
    instance.RegisterEventNotification(event_id_, handler, remote_pid_);

    // and when notify event message is received with the same event
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler has been called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventMessageDoesNotCallHandlerForDifferentEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler to track whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // when handler being registered for event
    instance.RegisterEventNotification(event_id_, handler, remote_pid_);

    // When notify event message is received for a different event
    ++event_id_.element_id_;
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler should be called
    EXPECT_FALSE(handler_called);
}

// ReregisterEventNotification()
TEST_F(MessagePassingServiceInstanceTest, ReregisterEventNotificationDoesNotRegisterNewNotification)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and client factory mock's Create() to not be called
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When ReregisterEventNotification is called on previously unregistered notification
    instance.ReregisterEventNotification(event_id_, remote_pid_);

    // Then NotifyEvent() for the event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest,
       ReregisterEventNotificationDoesNotAffectLocalRegistrationsWhenCalledWithRemotePid)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and event handler
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and notification being registered locally
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When ReregisterEventNotification is called for the same event but with remote pid
    instance.ReregisterEventNotification(event_id_, remote_pid_);

    // Given notification message received
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler should be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest,
       ReregisterEventNotificationDoesNotAffectLocalRegistrationsWhenCalledWithLocalPid)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and event handler
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and notification being registered locally
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When ReregisterEventNotification is called for the same event/local pid
    instance.ReregisterEventNotification(event_id_, local_pid_);

    // Given notification message received
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler should be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, ReregisterEventNotificationForTheSameEventPidCombinationDoesNotSendMessage)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // Given event notification being registered for remote pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When ReregisterEventNotification() is called with the same event and pid
    instance.ReregisterEventNotification(event_id_, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest, ReregisterEventNotificationWithDifferentNodeIdReplacesNodeIdAndSendsMessage)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client factory mock to be requested to create client connection twice
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Invoke([](auto&&...) {
            auto mock =
                score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());
            EXPECT_CALL(*mock, Send(::testing::_)).WillOnce(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));

            return mock;
        }));

    // Given event notification is registered
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When event notification for the same event is reregistered with a different pid
    instance.ReregisterEventNotification(event_id_, remote_pid_ + 5);
}

// NotifyOutdatedNode()
TEST_F(MessagePassingServiceInstanceTest, NotifyOutdatedNodeCreatesClientAndSendsMessage)
{
    RecordProperty("Verifies", "SCR-5898962, SCR-5899276, SCR-5899282");
    RecordProperty("Description", "Outdated Node Id notification is exchanged via message-passing");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // When NotifyOutdatedNodeId() is called for a previously unused target_node_id
    instance.NotifyOutdatedNodeId(remote_pid_, remote_pid_ + 2);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyOutdatedNodeDoesNotTerminateOnFailedSend)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // When NotifyOutdatedNodeId() is called for a previously unused target_node_id
    instance.NotifyOutdatedNodeId(remote_pid_, remote_pid_ + 2);
}

TEST_F(MessagePassingServiceInstanceTest, RegisterCallbackWithNoExistingHandlersCallbackNotInvoked)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<bool> callback_invoked{false};

    // When registering a callback with no existing handlers
    instance.RegisterEventNotificationExistenceChangedCallback(
        event_id_, [&callback_invoked]([[maybe_unused]] bool has_handlers) noexcept {
            callback_invoked.store(true);
        });

    // Then callback should NOT be invoked (optimization: no callback when no handlers exist)
    EXPECT_FALSE(callback_invoked.load());
}

TEST_F(MessagePassingServiceInstanceTest, RegisterCallbackWithExistingLocalHandlersCallbackInvokedWithTrue)
{
    // Given service instance with existing local handler
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    auto handler = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    std::atomic<bool> callback_invoked{false};
    std::atomic<bool> callback_value{false};

    // When registering a callback with existing local handlers
    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_invoked.store(true);
        callback_value.store(has_handlers);
    });

    // Then callback should be invoked with true
    EXPECT_TRUE(callback_invoked.load());
    EXPECT_TRUE(callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, RegisterCallbackWithExistingRemoteHandlersCallbackInvokedWithTrue)
{
    // Given service instance with existing remote handler
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Setup client connection mock for remote registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    std::atomic<bool> callback_invoked{false};
    std::atomic<bool> callback_value{false};

    // When registering a callback with existing remote handlers
    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_invoked.store(true);
        callback_value.store(has_handlers);
    });

    // Then callback should be invoked with true
    EXPECT_TRUE(callback_invoked.load());
    EXPECT_TRUE(callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, RegisterLocalHandlerCallbackInvokedWithTrue)
{
    // Given service instance with registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<bool> callback_invoked{false};
    std::atomic<bool> callback_value{false};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_invoked.store(true);
        callback_value.store(has_handlers);
    });

    // When registering first local handler
    auto handler = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // Then callback should be invoked with true
    EXPECT_TRUE(callback_invoked.load());
    EXPECT_TRUE(callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterLastLocalHandlerCallbackInvokedWithFalse)
{
    // Given service instance with one local handler and registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    auto handler = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    auto handler_id = instance.RegisterEventNotification(event_id_, handler, local_pid_);

    std::atomic<int> callback_count{0};
    std::atomic<bool> last_callback_value{true};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_count.fetch_add(1);
        last_callback_value.store(has_handlers);
    });

    callback_count.store(0);  // Reset after registration callback

    // When unregistering the last handler
    instance.UnregisterEventNotification(event_id_, handler_id, local_pid_);

    // Then callback should be invoked with false
    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_FALSE(last_callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, MultipleLocalHandlersCallbackOnlyOnFirstAndLast)
{
    // Given service instance with registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<int> callback_count{0};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_,
                                                               [&]([[maybe_unused]] bool has_handlers) noexcept {
                                                                   callback_count.fetch_add(1);
                                                               });

    // When registering multiple handlers
    auto handler1 = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    auto handler_id1 = instance.RegisterEventNotification(event_id_, handler1, local_pid_);
    EXPECT_EQ(callback_count.load(), 1);  // Called on first handler

    auto handler2 = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    auto handler_id2 = instance.RegisterEventNotification(event_id_, handler2, local_pid_);
    EXPECT_EQ(callback_count.load(), 1);  // Not called on second handler

    // When unregistering handlers
    instance.UnregisterEventNotification(event_id_, handler_id1, local_pid_);
    EXPECT_EQ(callback_count.load(), 1);  // Not called, still have one handler

    instance.UnregisterEventNotification(event_id_, handler_id2, local_pid_);
    EXPECT_EQ(callback_count.load(), 2);  // Called on last handler removal
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterCallbackNoMoreCallbacksInvoked)
{
    // Given service instance with registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<int> callback_count{0};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_,
                                                               [&]([[maybe_unused]] bool has_handlers) noexcept {
                                                                   callback_count.fetch_add(1);
                                                               });

    // When unregistering the callback
    instance.UnregisterEventNotificationExistenceChangedCallback(event_id_);

    // And then registering a handler
    auto handler = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // Then callback should not be invoked
    EXPECT_EQ(callback_count.load(), 0);
}

TEST_F(MessagePassingServiceInstanceTest, LocalAndRemoteHandlersMixedCallbackRespectsOverallState)
{
    // Given service instance with registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<int> callback_count{0};
    std::atomic<bool> last_callback_value{false};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_count.fetch_add(1);
        last_callback_value.store(has_handlers);
    });

    // When registering both local and remote handlers
    auto handler = std::make_shared<ScopedEventReceiveHandler>(scope_, []() noexcept {});
    auto handler_id = instance.RegisterEventNotification(event_id_, handler, local_pid_);
    EXPECT_EQ(callback_count.load(), 1);  // Called on first handler
    EXPECT_TRUE(last_callback_value.load());

    // Setup client connection mock for remote registration
    auto client_conn_mock =
        score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());
    EXPECT_CALL(*client_conn_mock, Send(::testing::_))
        .WillRepeatedly(testing::Return(score::cpp::expected_blank<score::os::Error>{}));
    EXPECT_CALL(*client_conn_mock, GetState()).WillRepeatedly(testing::Return(IClientConnection::State::kReady));
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_conn_mock))));

    auto remote_handler_id = instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    EXPECT_EQ(callback_count.load(), 1);  // Not called, already have handlers

    // When removing local handler (remote still exists)
    instance.UnregisterEventNotification(event_id_, handler_id, local_pid_);
    EXPECT_EQ(callback_count.load(), 1);  // Not called, still have remote handler

    // When removing remote handler (no handlers left)
    instance.UnregisterEventNotification(event_id_, remote_handler_id, remote_pid_);
    EXPECT_EQ(callback_count.load(), 2);  // Called when last handler removed
    EXPECT_FALSE(last_callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterNonExistentCallbackLogsWarning)
{
    // Given service instance without registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When unregistering a non-existent callback
    // Then it should log a warning but not crash
    instance.UnregisterEventNotificationExistenceChangedCallback(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, RemoteHandlerRegistrationInvokesCallback)
{
    // Given service instance with registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<bool> callback_invoked{false};
    std::atomic<bool> callback_value{false};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_invoked.store(true);
        callback_value.store(has_handlers);
    });

    // Setup client connection mock for remote registration
    EXPECT_CALL(client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // When registering first remote handler
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // Then callback should be invoked with true
    EXPECT_TRUE(callback_invoked.load());
    EXPECT_TRUE(callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, RemoteHandlerUnregistrationInvokesCallback)
{
    // Given service instance with one remote handler and registered callback
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Setup client connection mock for remote registration
    auto client_conn_mock =
        score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());
    EXPECT_CALL(*client_conn_mock, Send(::testing::_))
        .WillRepeatedly(testing::Return(score::cpp::expected_blank<score::os::Error>{}));
    EXPECT_CALL(*client_conn_mock, GetState()).WillRepeatedly(testing::Return(IClientConnection::State::kReady));
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_conn_mock))));

    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    std::atomic<int> callback_count{0};
    std::atomic<bool> last_callback_value{true};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_count.fetch_add(1);
        last_callback_value.store(has_handlers);
    });

    callback_count.store(0);  // Reset after registration callback

    // When unregistering the last remote handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);

    // Then callback should be invoked with false
    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_FALSE(last_callback_value.load());
}

// Additional tests to cover missing branches

TEST_F(MessagePassingServiceInstanceTest, RemoteHandlerRegistrationViaMessageInvokesCallbackWhenNoLocalHandlers)
{
    // Given service instance with registered callback but no handlers
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    std::atomic<int> callback_count{0};
    std::atomic<bool> callback_value{false};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_count.fetch_add(1);
        callback_value.store(has_handlers);
    });

    callback_count.store(0);  // Reset after registration

    // When receiving RegisterEventNotification message from remote node
    // This simulates a remote proxy registering for event notifications
    const auto message = Serialize(event_id_, MessageType::kRegisterEventNotifier);

    // Invoke the message callback via server connection (simulating remote registration)
    score::cpp::ignore = received_send_message_callback_(*server_connection_mock_, message);

    // Then callback should be invoked with true (covers line 282)
    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_TRUE(callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, RemoteHandlerUnregistrationViaMessageInvokesCallbackWhenNoLocalHandlers)
{
    // Given service instance with registered callback and one remote handler
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // First register a remote handler via message
    const auto register_message = Serialize(event_id_, MessageType::kRegisterEventNotifier);
    score::cpp::ignore = received_send_message_callback_(*server_connection_mock_, register_message);

    std::atomic<int> callback_count{0};
    std::atomic<bool> callback_value{true};

    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_count.fetch_add(1);
        callback_value.store(has_handlers);
    });

    callback_count.store(0);  // Reset after registration

    // When receiving UnregisterEventNotification message from remote node
    const auto unregister_message = Serialize(event_id_, MessageType::kUnregisterEventNotifier);
    score::cpp::ignore = received_send_message_callback_(*server_connection_mock_, unregister_message);

    // Then callback should be invoked with false (covers line 363)
    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_FALSE(callback_value.load());
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterEventNotificationWithNonExistingHandlerNoHandler)
{
    IMessagePassingService::HandlerRegistrationNoType registration_no{1U};
    // Given service instance WITHOUT any registered handler for event_id_
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Register a callback to verify it's NOT invoked when no handlers exist
    std::atomic<int> callback_count{0};
    std::atomic<bool> callback_value{false};
    instance.RegisterEventNotificationExistenceChangedCallback(event_id_, [&](bool has_handlers) noexcept {
        callback_count.fetch_add(1);
        callback_value.store(has_handlers);
    });

    callback_count.store(0);  // Reset after registration (callback not invoked since no handlers exist)

    // When trying to unregister a handler that was never registered
    instance.UnregisterEventNotification(event_id_, registration_no, local_pid_);

    // Then status change callback is NOT invoked (no actual state change occurred)
    EXPECT_EQ(callback_count.load(), 0);  // Callback should not be invoked
}

TEST_F(MessagePassingServiceInstanceTest, ScopedFunctionPreventsCallbackExecutionAfterDestruction)
{
    // Given the the received_send_message_callback captured before instance destruction
    MessageCallback captured_callback;
    {
        MessagePassingServiceInstance instance{
            quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

        // Capture the callback created during construction
        captured_callback = std::move(received_send_message_callback_);
    }
    // when the instance is destroyed (expiring the scope of the received_send_message_callback)

    // Then calling the captured callback, we don't crash.
    const auto message = Serialize(event_id_, MessageType::kNotifyEvent);
    score::cpp::ignore = captured_callback(*server_connection_mock_, message);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
