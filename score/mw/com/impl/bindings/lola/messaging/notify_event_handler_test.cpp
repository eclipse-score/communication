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
#include "score/mw/com/impl/bindings/lola/messaging/notify_event_handler.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_control_mock.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_outdated_nodeid.h"
#include "score/mw/com/message_passing/receiver_mock.h"
#include "score/mw/com/message_passing/sender_mock.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <score/optional.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <ctime>
#include <memory>
#include <numeric>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola::test
{

class NotifyEventHandlerAttorney
{
  public:
    NotifyEventHandlerAttorney(NotifyEventHandler& notify_event_handler) : notify_event_handler_{notify_event_handler}
    {
    }

    void RequestStopOnThreadPool()
    {
        notify_event_handler_.thread_pool_->Shutdown();
    }

  private:
    NotifyEventHandler& notify_event_handler_;
};

namespace
{

using ::testing::_;
using ::testing::An;
using ::testing::AnyNumber;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::ReturnRef;

const ElementFqId SOME_ELEMENT_FQ_ID{1, 1, 1, ServiceElementType::EVENT};
constexpr pid_t LOCAL_NODE_ID = 4444;
constexpr pid_t REMOTE_NODE_ID = 763;
constexpr pid_t NEW_REMOTE_NODE_ID = 764;
constexpr pid_t OUTDATED_REMOTE_NODE_ID = 551;

constexpr std::uint8_t kMaxReceiveHandlersPerEvent{5U};

struct NotifyEventCallbackCounterStore
{
    std::atomic<size_t> counter{};
    safecpp::Scope<> scope{};
    std::shared_ptr<ScopedEventReceiveHandler> handler{
        std::make_shared<ScopedEventReceiveHandler>(scope, [this]() noexcept {
            counter++;
        })};
};

MATCHER(IsRegisterMessage, "")
{
    return ((arg.id == static_cast<message_passing::MessageId>(MessageType::kRegisterEventNotifier)) &&
            (arg.payload == ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID)));
}

MATCHER(IsUnregisterMessage, "")
{
    return ((arg.id == static_cast<message_passing::MessageId>(MessageType::kUnregisterEventNotifier)) &&
            (arg.payload == ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID)));
}

MATCHER(IsNotifyEventMessage, "")
{
    return ((arg.id == static_cast<message_passing::MessageId>(MessageType::kNotifyEvent)) &&
            (arg.payload == ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID)));
}

MATCHER(IsOutdatedNodeIdMessage, "")
{
    return ((arg.id == static_cast<message_passing::MessageId>(MessageType::kOutdatedNodeId)) &&
            (arg.payload == OUTDATED_REMOTE_NODE_ID));
}

class NotifyEventHandlerFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(mp_control_mock_, GetNodeIdentifier()).WillByDefault(Return(LOCAL_NODE_ID));

        // Set up default behaviour when remote event notifications are registered
        ON_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kRegisterEventNotifier),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
            .WillByDefault(Invoke(this, &NotifyEventHandlerFixture::RegisterRegisterEventNotifierReceivedQmCB));
        ON_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kUnregisterEventNotifier),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
            .WillByDefault(Invoke(this, &NotifyEventHandlerFixture::RegisterUnregisterEventNotifierReceivedQmCB));
        ON_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kNotifyEvent),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
            .WillByDefault(Invoke(this, &NotifyEventHandlerFixture::RegisterNotifyEventReceivedQmCB));
        ON_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kOutdatedNodeId),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
            .WillByDefault(Invoke(this, &NotifyEventHandlerFixture::RegisterOutdatedNodeIdReceivedQmCB));
    }

    void TearDown() override {}

    NotifyEventHandlerFixture& WithMessagePassingSenders(std::vector<pid_t> node_ids)
    {
        for (auto node_id : node_ids)
        {
            auto inserted_element = sender_mock_map_.insert({node_id, std::make_shared<message_passing::SenderMock>()});
            EXPECT_TRUE(inserted_element.second);
            ON_CALL(mp_control_mock_, GetMessagePassingSender(_, node_id))
                .WillByDefault(Return(inserted_element.first->second));
        }
        return *this;
    }

    /// \brief create NotifyEventHandler uut
    /// \param asil_support false - only QM supported, true - QM and ASIL-B supported
    NotifyEventHandlerFixture& WithANotifyEventHandler(bool asil_support)
    {
        unit_.emplace(mp_control_mock_, asil_support, stop_token_);
        return *this;
    }

    void RegisterRegisterEventNotifierReceivedQmCB(message_passing::MessageId id,
                                                   message_passing::IReceiver::ShortMessageReceivedCallback callback)
    {
        EXPECT_EQ(id, static_cast<message_passing::MessageId>(MessageType::kRegisterEventNotifier));
        register_event_notifier_message_received_ = std::move(callback);
    }

    void RegisterUnregisterEventNotifierReceivedQmCB(message_passing::MessageId id,
                                                     message_passing::IReceiver::ShortMessageReceivedCallback callback)
    {
        EXPECT_EQ(id, static_cast<message_passing::MessageId>(MessageType::kUnregisterEventNotifier));
        unregister_event_notifier_message_received_ = std::move(callback);
    }

    void RegisterNotifyEventReceivedQmCB(message_passing::MessageId id,
                                         message_passing::IReceiver::ShortMessageReceivedCallback callback)
    {
        EXPECT_EQ(id, static_cast<message_passing::MessageId>(MessageType::kNotifyEvent));
        event_notify_message_received_ = std::move(callback);
    }

    void RegisterOutdatedNodeIdReceivedQmCB(message_passing::MessageId id,
                                            message_passing::IReceiver::ShortMessageReceivedCallback callback)
    {
        EXPECT_EQ(id, static_cast<message_passing::MessageId>(MessageType::kOutdatedNodeId));
        outdated_node_id_message_received_ = std::move(callback);
    }

    void RemoteRegisterEventNotificationIsReceived(QualityType, ElementFqId element_id, pid_t remote_node_id)
    {
        // when a RegisterEventNotification message has been received
        message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(element_id);
        register_event_notifier_message_received_(payload, remote_node_id);
    }

    void RemoteEventNotificationIsReceived(QualityType, ElementFqId element_id, pid_t remote_node_id)
    {
        // when a NotifyEventMessage (id = kNotifyEvent) is received for this event id
        message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(element_id);
        event_notify_message_received_(payload, remote_node_id);
    }

    score::cpp::stop_source source_{};
    score::cpp::stop_token stop_token_{source_.get_token()};

    score::mw::com::message_passing::ReceiverMock receiver_mock_{};
    std::unordered_map<pid_t, std::shared_ptr<message_passing::SenderMock>> sender_mock_map_{};
    ::testing::NiceMock<score::mw::com::impl::lola::MessagePassingControlMock> mp_control_mock_{};

    score::cpp::optional<NotifyEventHandler> unit_;

    QualityType last_asil_level_{QualityType::kInvalid};

    message_passing::IReceiver::ShortMessageReceivedCallback register_event_notifier_message_received_;
    message_passing::IReceiver::ShortMessageReceivedCallback unregister_event_notifier_message_received_;
    message_passing::IReceiver::ShortMessageReceivedCallback event_notify_message_received_;
    message_passing::IReceiver::ShortMessageReceivedCallback outdated_node_id_message_received_;

    NotifyEventCallbackCounterStore notify_event_callback_counter_store_local_{};
    NotifyEventCallbackCounterStore notify_event_callback_counter_store_remote_{};
};

TEST_F(NotifyEventHandlerFixture, Creation)
{
    // construction of NotifyEventHandler with ASIL support succeeds
    NotifyEventHandler unitWithAsil{mp_control_mock_, true, source_.get_token()};

    // construction of NotifyEventHandler without ASIL support succeeds.
    NotifyEventHandler unitWithoutAsil{mp_control_mock_, false, source_.get_token()};
}

TEST_F(NotifyEventHandlerFixture, RegisterQMReceiveCallbacks)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // expect, that callbacks for messages kRegisterEventNotifier
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kRegisterEventNotifier),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);
    // ... and kUnregisterEventNotifier get registered
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kUnregisterEventNotifier),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);
    // ... and kNotifyEvent get registered
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kNotifyEvent),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);
    // ... and kOutdatedNodeId get registered
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kOutdatedNodeId),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);

    // when calling RegisterMessageReceivedCallbacks
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);
}

TEST_F(NotifyEventHandlerFixture, RegisterASILReceiveCallbacks)
{
    // given a NotifyEventHandler with ASIL support
    WithANotifyEventHandler(true);

    // expect, that callbacks for messages kRegisterEventNotifier
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kRegisterEventNotifier),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);
    // ... and kUnregisterEventNotifier get registered
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kUnregisterEventNotifier),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);
    // ... and kNotifyEvent get registered
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kNotifyEvent),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);
    // ... and kOutdatedNodeId get registered
    EXPECT_CALL(receiver_mock_,
                Register(static_cast<std::underlying_type_t<MessageType>>(MessageType::kOutdatedNodeId),
                         (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .Times(1);

    // when calling RegisterMessageReceivedCallbacks
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_B, receiver_mock_);
}

TEST_F(NotifyEventHandlerFixture, RegisterNotification_LocalEvent)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // expecting that NO MessagePassingSender is retrieved which is required in order to send a
    // RegisterNotificationMessage
    EXPECT_CALL(mp_control_mock_, GetMessagePassingSender(_, _)).Times(0);

    // when registering a receive-handler for a local event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, RegisterNotification_RemoteEvent)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // expect, that one Send-call of a RegisterEventNotifier message takes place
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // when registering a receive-handler for a event on a remote node
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);
}

/**
 * \brief Basically same test case as above (RegisterNotification_RemoteEvent), but this time the message sending to
 *        remote node fails. UuT in this case logs a warning, but since ara::log has currently no mock support,
 *        we don't check that explicitly!
 */
TEST_F(NotifyEventHandlerFixture, RegisterNotification_RemoteEvent_SendError)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // expect, that one Send-call of a RegisterEventNotifier message takes place, which in this test returns an error.
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(10))));

    // when registering a receive-handler for a event on a remote node
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, RegisterMultipleNotification_RemoteEvent)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // expect, that a RegisterNotificationMessage is sent only once for the first registration
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())))
        .Times(1);

    // and that there is already a registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when there is an additional/2nd notification-registration for the same event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, std::make_shared<ScopedEventReceiveHandler>(), REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, RegisterMultipleNotificationNewNode_RemoteEvent)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID, NEW_REMOTE_NODE_ID});

    // expect that that a RegisterNotificationMessage is sent once for each node id
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // and there is already a registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when there is an additional/2nd notification-registration for the same event but now for a new/different node id
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, std::make_shared<ScopedEventReceiveHandler>(), NEW_REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, RegisteringMoreNotificationsForRemoteEventThanMaxAllowedStillSendsAllRegistrations)
{
    const auto number_of_registration_calls = kMaxReceiveHandlersPerEvent + 5;

    // Given `number_of_registration_calls` node ids
    std::vector<pid_t> node_ids(number_of_registration_calls);
    std::iota(node_ids.begin(), node_ids.end(), REMOTE_NODE_ID);

    // and given a NotifyEventHandler without ASIL support with message passing senders for each node id
    WithANotifyEventHandler(false).WithMessagePassingSenders(node_ids);

    // expect, that a Send-call of a RegisterEventNotifier message takes place for a sender associated with each node id
    // for every registration
    for (int i = 0; i < number_of_registration_calls; ++i)
    {
        EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID + i],
                    Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    }

    // when registering a receive-handler for an event with kMaxReceiveHandlersPerEvent + 5 different remote nodes
    for (int i = 0; i < number_of_registration_calls; ++i)
    {
        score::cpp::ignore = unit_.value().RegisterEventNotification(QualityType::kASIL_QM,
                                                              SOME_ELEMENT_FQ_ID,
                                                              notify_event_callback_counter_store_remote_.handler,
                                                              REMOTE_NODE_ID + i);
    }
}

TEST_F(NotifyEventHandlerFixture,
       NotifyingRemoteEventWhenRegisteringMoreThanMaxAllowedOnlyCallsMaxAllowedNumberOfHandlers)
{
    const auto number_of_registration_calls = kMaxReceiveHandlersPerEvent + 1;

    // Given `number_of_registration_calls` node ids
    std::vector<pid_t> node_ids(number_of_registration_calls);
    std::iota(node_ids.begin(), node_ids.end(), REMOTE_NODE_ID);

    // and given a NotifyEventHandler without ASIL support with message passing senders for each node id
    WithANotifyEventHandler(false).WithMessagePassingSenders(node_ids);

    // and that an event notification is registered for a remote event kMaxReceiveHandlersPerEvent + 1 times
    for (int i = 0; i < number_of_registration_calls; ++i)
    {
        score::cpp::ignore = unit_.value().RegisterEventNotification(QualityType::kASIL_QM,
                                                              SOME_ELEMENT_FQ_ID,
                                                              notify_event_callback_counter_store_remote_.handler,
                                                              REMOTE_NODE_ID + i);
    }

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);

    // Then the event-notification of the registered event will be called only for the maximum number of receive
    // handlers per event
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(notify_event_callback_counter_store_remote_.counter, kMaxReceiveHandlersPerEvent);
}

TEST_F(NotifyEventHandlerFixture, NotifyEvent_LocalReceiverOnly)
{
    RecordProperty("Verifies", "SCR-5898338, SCR-5898962, SCR-5899250");  // SWS_CM_00182
    RecordProperty("Description", "Callback is invoked from within messaging thread");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});
    // with a registered event-receive-handler/event-notification
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);

    // expect, that the event-notification has been called
    while (notify_event_callback_counter_store_remote_.counter != 1)
    {
        std::this_thread::yield();
    };
}

TEST_F(NotifyEventHandlerFixture,
       NotifyingLocalEventWhenRegisteringMoreThanMaxAllowedOnlyCallsMaxAllowedNumberOfHandlers)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // and that an event notification is registered for a local event kMaxReceiveHandlersPerEvent + 1 times
    const auto number_of_registration_calls = kMaxReceiveHandlersPerEvent + 1;
    for (int i = 0; i < number_of_registration_calls; ++i)
    {
        score::cpp::ignore = unit_.value().RegisterEventNotification(QualityType::kASIL_QM,
                                                              SOME_ELEMENT_FQ_ID,
                                                              notify_event_callback_counter_store_local_.handler,
                                                              LOCAL_NODE_ID);
    }

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);

    // Then the event-notification of the registered event will be called only for the maximum number of receive
    // handlers per event
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(notify_event_callback_counter_store_local_.counter, kMaxReceiveHandlersPerEvent);
}

TEST_F(NotifyEventHandlerFixture, NotifyingLocalEventWillNotCallHandlersWhenHandlersWereDestroyedByCaller)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // and that an event notification is registered for a local event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);

    // and that the shared_ptr containing the handler owned by the caller is destroyed
    notify_event_callback_counter_store_local_.handler.reset();

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);

    // Then no event-notifications of the registered event will be called
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(notify_event_callback_counter_store_local_.counter, 0U);
}

TEST_F(NotifyEventHandlerFixture, UnregisterNotification_LocalEvent)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // with a registered event-receive-handler/event-notification
    const auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);

    // when unregistering the receive-handler
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, LOCAL_NODE_ID);
    // and then notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);

    // expect, that the event-notification has NOT been called
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(notify_event_callback_counter_store_local_.counter, 0);
}

TEST_F(NotifyEventHandlerFixture, UnregisterNotification_LocalEvent_Unkown)
{
    IMessagePassingService::HandlerRegistrationNoType unknownregistration_number{9999999};
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // with a registered event-receive-handler/event-notification
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);

    // when unregistering a receive-handler with an unknown/non-existing registration number
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, unknownregistration_number, LOCAL_NODE_ID);

    // and then notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);

    // expect, that the event-notification of the registered event (not yet unregistered!) has still been called.
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(notify_event_callback_counter_store_local_.counter, 1);
}

/**
 * \brief Basically same test case as above (UnregisterNotification_LocalEvent), but this time the Unregister call
 *        is done with another (wrong) remote node id as used for the Register call!
 *        UuT in this case logs a warning, but since ara::log has currently no mock support, we don't check that
 *        explicitly!
 */
TEST_F(NotifyEventHandlerFixture, UnregisterNotification_LocalEvent_WrongNodeId)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // expect, that NO Send-call of a UnregisterEventNotifier message takes place
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID], Send(An<const message_passing::ShortMessage&>())).Times(0);

    // with a registered event-receive-handler/event-notification for a local event
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);

    // when unregistering the receive-handler with a different (wrong) node id
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, UnregisterNotification_RemoteEvent)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // expect that a message is sent for the RegisterEventNotifier and then the UnregisterEventNotifier messages
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsUnregisterMessage())));

    // with a registered event-receive-handler/event-notification for a remote event
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when unregistering the receive-handler
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, UnregisterNotification_RemoteEvent_UnknownNode)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID, NEW_REMOTE_NODE_ID});

    // expect a Send-call of a RegisterEventNotifier message takes place
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // with a registered event-receive-handler/event-notification for a remote event on node REMOTE_NODE_ID
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when unregistering the receive-handler for a remote node, for which no receive-handler has been yet registered
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, NEW_REMOTE_NODE_ID);
}

/**
 * \brief Basically same test case as above (UnregisterNotification_RemoteEvent), but this time the message sending to
 *        remote node fails. UuT in this case logs a warning, but since ara::log has currently no mock support,
 *        we don't check that explicitly!
 */
TEST_F(NotifyEventHandlerFixture, UnregisterNotification_RemoteEvent_SendError)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // expect that a message is sent for the RegisterEventNotifier and then the UnregisterEventNotifier messages which
    // returns an error
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsUnregisterMessage())))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(10))));

    // with a registered event-receive-handler/event-notification for a remote event
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when unregistering the receive-handler
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, CallingUnregisterEventNotificationWillNotSendUnregisterIfRegistrationsStillExist)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // Expecting that a message is sent for the first call to RegisterEventNotifier (Note. this is not an expectation
    // that is being tested in this test, but gmock requires that we add expectations for ALL calls to an API if we have
    // a single EXPECT_CALL on that API)
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // Expecting that an UnregisterEventNotifier message is never sent
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsUnregisterMessage())))
        .Times(0);

    // given that an event notification is registered for a remote event twice
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when unregistering the receive-handler for the first registration only
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, CallingUnregisterEventNotificationWithoutRegistrationWillNotSendUnregister)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // Expecting that an UnregisterEventNotifier message is never sent
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsUnregisterMessage())))
        .Times(0);

    // When unregistering the receive-handler having never registered one
    IMessagePassingService::HandlerRegistrationNoType invalid_registration_no{100U};
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, invalid_registration_no, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, CallingUnregisterEventNotificationTwiceWillSendUnregisterOnlyOnce)
{
    // Given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // Expecting that a message is sent for the call to RegisterEventNotifier (Note. this is not an expectation
    // that is being tested in this test, but gmock requires that we add expectations for ALL calls to an API if we have
    // a single EXPECT_CALL on that API)
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // and that an UnregisterEventNotifier message is sent only once
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsUnregisterMessage())))
        .Times(1);

    // given that an event notification is registered for a remote event
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when calling UnregisterEventNotification twice with the same registration_number
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, REMOTE_NODE_ID);
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, ReregisterNotification_LocalEvent_OK)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // expecting that NO MessagePassingSender is retrieved which is required in order to send a
    // RegisterNotificationMessage
    EXPECT_CALL(mp_control_mock_, GetMessagePassingSender(_, _)).Times(0);

    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);

    // when re-registering the same event with the same local id which is already registered
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, LOCAL_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, ReregisterNotification_RemoteEvent_OK)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID, NEW_REMOTE_NODE_ID});

    // expect, that a Send-call of a RegisterEventNotifier message takes place once on registration and again on
    // re-registration with the new node id
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // and there is already a registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when re-registering the same event for a new remote id
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, NEW_REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, ReregisterNotification_RemoteEvent_2nd)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID, NEW_REMOTE_NODE_ID});

    // expect, that one Send-call of a RegisterEventNotifier message takes place on registration and again on
    // re-registration with the new node id but not on the second re-registration
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // and there is already a registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when re-registering the same event for a new remote id the 1st time
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, NEW_REMOTE_NODE_ID);

    // and when a 2nd Reregistration happens for the same event/node-id
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, NEW_REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture,
       CallingReregisterEventNotificationForEventThatWasNeverRegisteredDoesNotSendNotification)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({NEW_REMOTE_NODE_ID});

    // Expecting that Send is never called
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID], Send(An<const message_passing::ShortMessage&>())).Times(0);

    // when re-registering an event that was never registered
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, NEW_REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture,
       CallingReregisterEventNotificationForEventThatWasRegisteredLocallyDoesNotSendNotification)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({NEW_REMOTE_NODE_ID});

    // Expecting that Send is never called
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID], Send(An<const message_passing::ShortMessage&>())).Times(0);

    // and there is already a registered event notification for a local event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_local_.handler, LOCAL_NODE_ID);

    // when re-registering an event that was registered locally
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, NEW_REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, ReregisterNotification_Unregister)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID, NEW_REMOTE_NODE_ID});

    // expect, that one Send-call of a RegisterEventNotifier message takes place on registration and again on
    // re-registration with the new node id
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsRegisterMessage())));

    // expect, that one Send-call of a UnregisterEventNotifier message takes place
    EXPECT_CALL(*sender_mock_map_[NEW_REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsUnregisterMessage())));

    // and there is already a registered event notification for a remote event
    auto registration_number = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when re-registering the same event for a new remote id
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, NEW_REMOTE_NODE_ID);

    // when Unregister is called again for the new/re-registered node id
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, registration_number, NEW_REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, RegisteredEventNotificationHandlerCalledWhenEventNotificationReceived)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // and given that receive handlers have been registered with a receiver
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and an event notification handler is registered
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when an event notification from a remote node is received
    RemoteEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);

    // expect, that the event-notification of the registered event has been called.
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(notify_event_callback_counter_store_remote_.counter, 1);
}

TEST_F(NotifyEventHandlerFixture, NotifyEvent_RemoteReceiverOnly)
{
    // SWS_CM_00182
    RecordProperty("Verifies", "SCR-5898338, SCR-5898962, SCR-5899250, SCR-5899276, SCR-5899282");
    RecordProperty("Description", "Remote receiver is notified via Message Passing.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and a registered event notification of a remote node
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);

    // and expect, that a NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsNotifyEventMessage())));

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, WhenRegisterEventNotificationIsReceivedTwice)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and a registered event notification of a remote node is received twice
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);

    // expecting that a NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsNotifyEventMessage())))
        .Times(1);

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

/**
 * \brief Basically same test case as above (NotifyEvent_RemoteReceiverOnly), but this time the message sending to
 *        remote node fails. UuT in this case logs a warning, but since ara::log has currently no mock support,
 *        we don't check that explicitly!
 */
TEST_F(NotifyEventHandlerFixture, NotifyEvent_RemoteReceiverOnly_SendError)
{
    // SWS_CM_00182
    RecordProperty("Verifies", "SCR-5898338, SCR-5898962, SCR-5899250, SCR-5899276, SCR-5899282");
    RecordProperty("Description", "Remote receiver is notified via Message Passing, but notification fails.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and a registered event notification of a remote node
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);

    // and expect, that a NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID, but sending fails in this
    // test
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsNotifyEventMessage())))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(10))));

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, NotifyEvent_HighNumberRemoteReceiversOnly)
{
    const auto number_of_node_ids = 30U;

    // Given `number_of_registration_calls` node ids
    std::vector<pid_t> node_ids(number_of_node_ids);
    std::iota(node_ids.begin(), node_ids.end(), REMOTE_NODE_ID);

    // and given a NotifyEventHandler without ASIL support with message passing senders for each node id
    WithANotifyEventHandler(false).WithMessagePassingSenders(node_ids);

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and expect, that a NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID for each node
    for (auto node_id : node_ids)
    {
        EXPECT_CALL(*sender_mock_map_[node_id],
                    Send(Matcher<const message_passing::ShortMessage&>(IsNotifyEventMessage())));
    }

    // and a high number of registered event notification of different remote nodes
    // Note: Count is 30 here as the impl. internally copies up to 20 node_identifiers into a temp buffer to
    // do the processing later after unlock(). This test with 30 nodes forces code-paths, where tmp-buffer has to be
    // refilled.
    for (auto node_id : node_ids)
    {
        RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, node_id);
    }
    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, NotifyEventMessageOnlySentForMaxSupportedRemoteNodes)
{
    const std::size_t max_number_of_node_ids_can_be_copied = 20U * 255U;
    const auto number_of_node_ids = max_number_of_node_ids_can_be_copied + 1U;

    // Given `number_of_registration_calls` node ids
    std::vector<pid_t> node_ids(number_of_node_ids);
    std::iota(node_ids.begin(), node_ids.end(), REMOTE_NODE_ID);

    // and given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // with message passing senders for each node id. Note. We use a single sender mock for all senders (regardless of
    // node id) since this test uses a huge number of senders (i.e. 20U * 255U) which leads to test timeouts when
    // running the test with spp_address_and_ub_sanitizer. NotifyEvent_HighNumberRemoteReceiversOnly checks that
    // separate message passing senders are used for each node id.
    auto sender_mock{std::make_shared<message_passing::SenderMock>()};
    ON_CALL(mp_control_mock_, GetMessagePassingSender(_, _)).WillByDefault(Return(sender_mock));

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and expect, that a NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID only the first n times
    // (where n is the maximum number of node ids that can be copied).
    EXPECT_CALL(*sender_mock, Send(Matcher<const message_passing::ShortMessage&>(IsNotifyEventMessage())))
        .Times(max_number_of_node_ids_can_be_copied);

    // and a high number of registered event notification of different remote nodes
    // Note: Count is 20 * 255 + 1 here as the impl. internally copies up to 20 node_identifiers into a temp buffer on
    // every iteration and does this max 255 times.
    for (auto node_id : node_ids)
    {
        RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, node_id);
    }

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, ReceiveEventNotification_OneNotifier)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and there is a locally registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // when a NotifyEventMessage (id = kNotifyEvent) is received for this event id
    message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID);
    event_notify_message_received_(payload, REMOTE_NODE_ID);

    // expect, that notify_event_callback_counter_ is 1
    EXPECT_EQ(notify_event_callback_counter_store_remote_.counter, 1);
}

TEST_F(NotifyEventHandlerFixture, StopBeforeReceiveEventNotification_OneNotifier)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and there is a locally registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // and a NotifyEventMessage (id = kNotifyEvent) is received for this event id
    message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID);
    // when requesting the stop tocken to stop
    source_.request_stop();
    event_notify_message_received_(payload, REMOTE_NODE_ID);

    // expect, that notify_event_callback_counter_ is 0
    EXPECT_EQ(notify_event_callback_counter_store_remote_.counter, 0);
}

TEST_F(NotifyEventHandlerFixture, ReceiveEventNotification_ZeroNotifier)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false);

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // when a NotifyEventMessage (id = kNotifyEvent) is received for this event id, although we don't have any
    // local interested receiver (proxy-event), which is basically unexpected, but can arise because of an acceptable
    // race-condition ...
    message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID);
    event_notify_message_received_(payload, REMOTE_NODE_ID);

    // expect, that both local and remove counters are 0
    EXPECT_EQ(notify_event_callback_counter_store_local_.counter, 0);
    EXPECT_EQ(notify_event_callback_counter_store_remote_.counter, 0);
}

TEST_F(NotifyEventHandlerFixture, ReceiveEventNotification_TwoNotifier)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and there is already one locally registered event notification for a remote event
    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, notify_event_callback_counter_store_remote_.handler, REMOTE_NODE_ID);

    // and we register a 2nd one
    NotifyEventCallbackCounterStore second_event_receive_handler_counter_store{};

    score::cpp::ignore = unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, second_event_receive_handler_counter_store.handler, REMOTE_NODE_ID);

    // when a NotifyEventMessage (id = kNotifyEvent) is received for this event id
    message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID);
    event_notify_message_received_(payload, REMOTE_NODE_ID);

    // expect, that the counter for each notify event handler is 1
    EXPECT_EQ(second_event_receive_handler_counter_store.counter, 1);
    EXPECT_EQ(notify_event_callback_counter_store_remote_.counter, 1);
}

TEST_F(NotifyEventHandlerFixture, ReceiveUnregisterEventNotification)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // expect, that no notifyEventMessage is sent to remote node
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID], Send(An<const message_passing::ShortMessage&>())).Times(0);

    // and a registered event notification of a remote node
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);

    // after a UnregisterEventNotificationMessage (id = kUnregisterEventNotifier) is received for this event id
    message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID);
    unregister_event_notifier_message_received_(payload, REMOTE_NODE_ID);
    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

/* \brief This test case is the same as above (ReceiveUnregisterEventNotification), but this time we have not even
 *        an active event update notification registered by the remote node. UuT will in this case log a warning and
 *        do nothing, but since ara::log has currently no mock support, we don't check that.
 **/
TEST_F(NotifyEventHandlerFixture, ReceiveUnregisterEventNotification_WithoutActualRegistration)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // expect, that no notifyEventMessage is sent to remote node
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID], Send(An<const message_passing::ShortMessage&>())).Times(0);

    // after a UnregisterEventNotificationMessage (id = kUnregisterEventNotifier) is received for this event id
    message_passing::ShortMessagePayload payload = ElementFqIdToShortMsgPayload(SOME_ELEMENT_FQ_ID);
    unregister_event_notifier_message_received_(payload, REMOTE_NODE_ID);

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, ReceiveOutdatedNodeIdMessage_ExistingNodeId)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({OUTDATED_REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and there has been registered an event-notification by a remote node id
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, OUTDATED_REMOTE_NODE_ID);

    // when an OutdatedNodeIdMessage (id = kOutdatedNodeId) is received for this OUTDATED_REMOTE_NODE_ID
    message_passing::ShortMessagePayload payload = OUTDATED_REMOTE_NODE_ID;
    outdated_node_id_message_received_(payload, REMOTE_NODE_ID);

    // then NO notification message is sent to OUTDATED_REMOTE_NODE_ID anymore
    // expect that GetMessagePassingSender() NOT to be called
    EXPECT_CALL(mp_control_mock_, GetMessagePassingSender(QualityType::kASIL_QM, OUTDATED_REMOTE_NODE_ID)).Times(0);

    // and expect, that NO NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID
    EXPECT_CALL(*sender_mock_map_[OUTDATED_REMOTE_NODE_ID], Send(An<const message_passing::ShortMessage&>())).Times(0);

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, ReceiveOutdatedNodeIdMessage_NoExistingNodeId)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // with registered receive-handlers
    unit_.value().RegisterMessageReceivedCallbacks(QualityType::kASIL_QM, receiver_mock_);

    // and there has been registered an event-notification by a remote node id
    RemoteRegisterEventNotificationIsReceived(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, REMOTE_NODE_ID);

    // when an OutdatedNodeIdMessage (id = kOutdatedNodeId) is received for a different OUTDATED_REMOTE_NODE_ID
    message_passing::ShortMessagePayload payload = OUTDATED_REMOTE_NODE_ID;
    outdated_node_id_message_received_(payload, REMOTE_NODE_ID);

    // and expect, that a NotifyEventUpdateMessage is sent out for event SOME_ELEMENT_FQ_ID
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsNotifyEventMessage())));

    // when notifying the event
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(NotifyEventHandlerFixture, SendOutdatedNodeIdMessage)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // and expect, that a OutdatedNodeIdMessage is sent with OUTDATED_REMOTE_NODE_ID
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsOutdatedNodeIdMessage())));

    // when notifying OUTDATED_REMOTE_NODE_ID as outdated node id towards REMOTE_NODE_ID
    unit_.value().NotifyOutdatedNodeId(QualityType::kASIL_QM, OUTDATED_REMOTE_NODE_ID, REMOTE_NODE_ID);
}

TEST_F(NotifyEventHandlerFixture, SendingOutdatedNodeIdMessageWillNotTerminateWhenSendCallReturnsError)
{
    // given a NotifyEventHandler without ASIL support
    WithANotifyEventHandler(false).WithMessagePassingSenders({REMOTE_NODE_ID});

    // and expect, that a OutdatedNodeIdMessage is sent with OUTDATED_REMOTE_NODE_ID which returns an error
    EXPECT_CALL(*sender_mock_map_[REMOTE_NODE_ID],
                Send(Matcher<const message_passing::ShortMessage&>(IsOutdatedNodeIdMessage())))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(10))));

    // when notifying OUTDATED_REMOTE_NODE_ID as outdated node id towards REMOTE_NODE_ID
    unit_.value().NotifyOutdatedNodeId(QualityType::kASIL_QM, OUTDATED_REMOTE_NODE_ID, REMOTE_NODE_ID);

    // Then the program does not terminate
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
