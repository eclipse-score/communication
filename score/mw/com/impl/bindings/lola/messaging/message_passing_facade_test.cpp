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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_facade.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_control_mock.h"
#include "score/mw/com/impl/bindings/lola/messaging/notify_event_handler_mock.h"
#include "score/mw/com/impl/bindings/lola/messaging/notify_event_handler_mock_facade.h"
#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"
#include "score/mw/com/message_passing/receiver_factory.h"
#include "score/mw/com/message_passing/receiver_mock.h"
#include "score/mw/com/message_passing/sender_mock.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <utility>

namespace score::mw::com::impl::lola::test
{

class MessagePassingFacadeAttorney
{
  public:
    explicit MessagePassingFacadeAttorney(MessagePassingFacade& message_passing_facade)
        : message_passing_facade_{message_passing_facade}
    {
    }

    const MessagePassingFacade::MessageReceiveCtrl& GetMsgReceiveCtrl(const QualityType asil_level)
    {
        return asil_level == QualityType::kASIL_B ? message_passing_facade_.msg_receiver_asil_b_
                                                  : message_passing_facade_.msg_receiver_qm_;
    }

  private:
    MessagePassingFacade& message_passing_facade_;
};

using ::testing::_;
using ::testing::An;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::Return;

constexpr pid_t OUR_PID = 4444;
constexpr std::int32_t ARBITRARY_POSIX_ERROR{10};
const ElementFqId SOME_ELEMENT_FQ_ID{1, 1, 1, ElementType::EVENT};

class ThreadHWConcurrencyMock : public ThreadHWConcurrencyIfc
{
  public:
    MOCK_METHOD(unsigned int, hardware_concurrency, (), (const, noexcept, override));
};

class MessagePassingFacadeFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        message_passing::ReceiverFactory::InjectReceiverMock(&receiver_mock_);
    }

    void TearDown() override {}

    void PrepareFacadeWithListenError()
    {
        MessagePassingFacade::AsilSpecificCfg asilCfg{10, std::vector<uid_t>{1, 2, 3}};
        // expect GetNodeIdentifier() is called to determine default node_identifier
        EXPECT_CALL(message_passing_control_mock_, GetNodeIdentifier()).WillOnce(Return(OUR_PID));
        // expect CreateMessagePassingName() is called to determine receiver name
        EXPECT_CALL(message_passing_control_mock_, CreateMessagePassingName(QualityType::kASIL_QM, OUR_PID))
            .WillOnce(Return("bla"));
        EXPECT_CALL(receiver_mock_, Register(_, (An<message_passing::IReceiver::MediumMessageReceivedCallback>())))
            .WillRepeatedly(Return());
        EXPECT_CALL(receiver_mock_, Register(_, (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
            .WillRepeatedly(Return());
        // expect that StartListening() is called and returns an error
        EXPECT_CALL(receiver_mock_, StartListening())
            .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ARBITRARY_POSIX_ERROR))));

        MessagePassingFacade facade{stop_source_,
                                    std::make_unique<NotifyEventHandlerMockFacade>(notify_event_handler_mock_),
                                    message_passing_control_mock_,
                                    asilCfg,
                                    score::cpp::nullopt};
    }

    void PrepareFacade(bool also_activate_asil)
    {
        MessagePassingFacade::AsilSpecificCfg asilCfg{10, std::vector<uid_t>{1, 2, 3}};
        uint8_t numListeners = also_activate_asil ? 2 : 1;

        // expect GetNodeIdentifier() is called to determine default node_identifier
        EXPECT_CALL(message_passing_control_mock_, GetNodeIdentifier())
            .Times(numListeners)
            .WillRepeatedly(Return(OUR_PID));
        // expect CreateMessagePassingName() is called to determine receiver name
        EXPECT_CALL(message_passing_control_mock_, CreateMessagePassingName(QualityType::kASIL_QM, OUR_PID))
            .WillOnce(Return("bla"));
        if (also_activate_asil)
        {
            // expect CreateMessagePassingName() is called to determine receiver name
            EXPECT_CALL(message_passing_control_mock_, CreateMessagePassingName(QualityType::kASIL_B, OUR_PID))
                .WillOnce(Return("blub"));
        }
        // we expect several call to Register at the receiver. Detailed verification of calls are done in the
        // specific handler unit tests (NotifyEventHandler, SubscribeEventHandler) as those registrations are done by
        // them. This coarse expectation is given here only to avoid misleading GMOCK WARNINGS regarding
        // "Uninteresting mock function call"
        EXPECT_CALL(receiver_mock_, Register(_, (An<message_passing::IReceiver::MediumMessageReceivedCallback>())))
            .WillRepeatedly(Return());
        EXPECT_CALL(receiver_mock_, Register(_, (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
            .WillRepeatedly(Return());

        // expect that StartListening() is called  successfully on created receivers
        EXPECT_CALL(receiver_mock_, StartListening())
            .Times(numListeners)
            .WillRepeatedly(Return(score::cpp::expected_blank<score::os::Error>{}));

        // when creating our MessagePassingFacade
        unit_.emplace(
            stop_source_,
            std::make_unique<NotifyEventHandlerMockFacade>(notify_event_handler_mock_),
            message_passing_control_mock_,
            asilCfg,
            also_activate_asil ? score::cpp::optional<MessagePassingFacade::AsilSpecificCfg>{asilCfg} : score::cpp::nullopt);
    }

    message_passing::ReceiverMock receiver_mock_{};
    MessagePassingControlMock message_passing_control_mock_{};
    ThreadHWConcurrencyMock concurrency_mock_{};
    score::cpp::stop_source stop_source_{};

    NotifyEventHandlerMock notify_event_handler_mock_{};
    std::optional<MessagePassingFacade> unit_;
};

/// \brief Test for heap allocation is needed to stimulate "deleting Dtor" for gcov func coverage.
TEST_F(MessagePassingFacadeFixture, CreationQMOnlyHeap)
{
    MessagePassingFacade::AsilSpecificCfg asilCfg{10, std::vector<uid_t>{1, 2, 3}};
    // expect GetNodeIdentifier() is called to determine default node_identifier
    EXPECT_CALL(message_passing_control_mock_, GetNodeIdentifier()).Times(1).WillRepeatedly(Return(OUR_PID));
    // expect CreateMessagePassingName() is called to determine receiver name
    EXPECT_CALL(message_passing_control_mock_, CreateMessagePassingName(QualityType::kASIL_QM, OUR_PID))
        .WillOnce(Return("bla"));
    // we expect several call to Register at the receiver. Detailed verification of calls are done in the
    // specific handler unit tests (NotifyEventHandler, SubscribeEventHandler) as those registrations are done by
    // them. This coarse expectation is given here only to avoid misleading GMOCK WARNINGS regarding
    // "Uninteresting mock function call"
    EXPECT_CALL(receiver_mock_, Register(_, (An<message_passing::IReceiver::MediumMessageReceivedCallback>())))
        .WillRepeatedly(Return());
    EXPECT_CALL(receiver_mock_, Register(_, (An<message_passing::IReceiver::ShortMessageReceivedCallback>())))
        .WillRepeatedly(Return());

    // expect that StartListening() is called  successfully on created receivers
    EXPECT_CALL(receiver_mock_, StartListening())
        .Times(1)
        .WillRepeatedly(Return(score::cpp::expected_blank<score::os::Error>{}));

    // when creating our MessagePassingFacade
    auto unit_on_heap = std::make_unique<MessagePassingFacade>(
        stop_source_,
        std::make_unique<NotifyEventHandlerMockFacade>(notify_event_handler_mock_),
        message_passing_control_mock_,
        asilCfg,
        score::cpp::nullopt);
    EXPECT_TRUE(unit_on_heap);
}

TEST_F(MessagePassingFacadeFixture, CreationQMAndAsil)
{
    // we have a Facade created for QM and ASIL-B with successful listening calls
    PrepareFacade(true);
}

TEST_F(MessagePassingFacadeFixture, ListeningFailure)
{
    // we expect a death/termination in case we create a Facade for QM with error/failure on message_queue listening
    // call.
    EXPECT_DEATH(PrepareFacadeWithListenError(), ".*");
}

TEST_F(MessagePassingFacadeFixture, NotifyEventWillDispatchToNotifyEventHandler)
{
    // Given a Facade created for QM only
    PrepareFacade(false);

    // Expecting that NotifyEvent will be called on the NotifyEventHandlerMock
    EXPECT_CALL(notify_event_handler_mock_, NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID));

    // when calling NotifyEvent
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

TEST_F(MessagePassingFacadeFixture, NotifyOutdatedNodeIdWillDispatchToNotifyEventHandler)
{
    // Given a Facade created for QM only
    PrepareFacade(false);

    // Expecting that NotifyOutdatedNodeId will be called on the NotifyEventHandlerMock
    const pid_t target_node_id{1};
    const pid_t outdated_node_id{42};
    EXPECT_CALL(notify_event_handler_mock_,
                NotifyOutdatedNodeId(QualityType::kASIL_QM, outdated_node_id, target_node_id));

    // when calling NotifyOutdatedNodeId
    unit_.value().NotifyOutdatedNodeId(QualityType::kASIL_QM, outdated_node_id, target_node_id);
}

TEST_F(MessagePassingFacadeFixture, RegisterEventNotificationWillDispatchToNotifyEventHandler)
{
    safecpp::Scope<> event_receive_handler_scope{};
    auto eventUpdateNotificationHandler =
        std::make_shared<ScopedEventReceiveHandler>(event_receive_handler_scope, []() noexcept {
            return;
        });

    // Given a Facade created for QM only
    PrepareFacade(false);

    // Expecting that RegisterEventNotification will be called on the NotifyEventHandlerMock
    EXPECT_CALL(notify_event_handler_mock_,
                RegisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, _, OUR_PID));

    // when calling RegisterEventNotification
    unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, std::move(eventUpdateNotificationHandler), OUR_PID);
}

TEST_F(MessagePassingFacadeFixture, ReregisterEventNotificationWillDispatchToNotifyEventHandler)
{
    // Given a Facade created for QM only
    PrepareFacade(false);

    // Expecting that ReregisterEventNotification will be called on the NotifyEventHandlerMock
    EXPECT_CALL(notify_event_handler_mock_,
                ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, OUR_PID));

    // when calling ReregisterEventNotification
    unit_.value().ReregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, OUR_PID);
}

TEST_F(MessagePassingFacadeFixture, UnregisterEventNotificationWillDispatchToNotifyEventHandler)
{
    // Given a Facade created for QM only
    PrepareFacade(false);

    // Expecting that UnregisterEventNotification will be called on the NotifyEventHandlerMock
    IMessagePassingService::HandlerRegistrationNoType invalid_registration_no = 7882;
    EXPECT_CALL(
        notify_event_handler_mock_,
        UnregisterEventNotification(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, invalid_registration_no, OUR_PID));

    // when calling UnregisterEventNotification, we will cover call forwarding to NotifyEventHandler
    unit_.value().UnregisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, invalid_registration_no, OUR_PID);
}

TEST_F(MessagePassingFacadeFixture, DifferentExecutorsForReceivers)
{
    RecordProperty("Verifies", "SCR-5899265");
    RecordProperty("Description",
                   "Checks, that different executors (and therefore threads) are used for msg-receivers");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // we have a Facade created for QM and ASIL-B, which then has two receivers (ASIL_QM and ASIL_B)
    PrepareFacade(true);

    // via our attorney helper, we can read out the internally created MessageReceiveCtrls
    auto attorney = MessagePassingFacadeAttorney(unit_.value());
    auto& rec_asil_b = attorney.GetMsgReceiveCtrl(QualityType::kASIL_B);
    auto& rec_asil_qm = attorney.GetMsgReceiveCtrl(QualityType::kASIL_QM);

    EXPECT_NE(rec_asil_b.thread_pool_.get(), rec_asil_qm.thread_pool_.get());
}

}  // namespace score::mw::com::impl::lola::test
