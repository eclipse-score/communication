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
#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"
#include "score/mw/com/message_passing/receiver_factory.h"
#include "score/mw/com/message_passing/receiver_mock.h"
#include "score/mw/com/message_passing/sender_mock.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <score/optional.hpp>

#include <gtest/gtest.h>

#include <memory>
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

        MessagePassingFacade facade{message_passing_control_mock_, asilCfg, score::cpp::nullopt};
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
            message_passing_control_mock_,
            asilCfg,
            also_activate_asil ? score::cpp::optional<MessagePassingFacade::AsilSpecificCfg>{asilCfg} : score::cpp::nullopt);
    }

    message_passing::ReceiverMock receiver_mock_{};
    MessagePassingControlMock message_passing_control_mock_{};
    ThreadHWConcurrencyMock concurrency_mock_{};

    score::cpp::optional<MessagePassingFacade> unit_;
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
    auto unit_on_heap = std::make_unique<MessagePassingFacade>(message_passing_control_mock_, asilCfg, score::cpp::nullopt);
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

// test case tests forwarding of call of NotifyEvent() to
// NotifyEventHandler::NotifyEvent().
// Since we do NOT want to introduce a mock for the NotifyEventHandler owned by the MessagePassingFacade, because
// it would introduce polymorphism and injection APIs in the MessagePassingFacade only for testing this simple call-
// forwarding for coverage completeness, we just stimulate the simplest possible call to verify coverage.
TEST_F(MessagePassingFacadeFixture, NotifyEvent)
{
    // we have a Facade created for QM only
    PrepareFacade(false);

    // expect, that GetNodeIdentifier() will get called (by the internal NotifyEventHandler owned bei UuT)
    EXPECT_CALL(message_passing_control_mock_, GetNodeIdentifier()).WillOnce(Return(OUR_PID));

    // when calling NotifyEvent, we will cover call forwarding to NotifyEventHandler
    unit_.value().NotifyEvent(QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID);
}

// test case tests forwarding of call of NotifyOutdatedNodeId() to
// NotifyEventHandler::NotifyOutdatedNodeId().
// Since we do NOT want to introduce a mock for the NotifyEventHandler owned by the MessagePassingFacade, because
// it would introduce polymorphism and injection APIs in the MessagePassingFacade only for testing this simple call-
// forwarding for coverage completeness, we just stimulate the simplest possible call to verify coverage.
TEST_F(MessagePassingFacadeFixture, NotifyOutdatedNodeId)
{
    // we have a Facade created for QM only
    PrepareFacade(false);

    const pid_t target_node_id{1};
    const pid_t outdated_node_id{42};

    // expect, that GetMessagePassingSender() will get called (by the internal NotifyEventHandler owned bei UuT)
    EXPECT_CALL(message_passing_control_mock_, GetMessagePassingSender(QualityType::kASIL_QM, target_node_id))
        .WillOnce(Return(std::make_shared<message_passing::SenderMock>()));

    // when calling NotifyEvent, we will cover call forwarding to NotifyEventHandler
    unit_.value().NotifyOutdatedNodeId(QualityType::kASIL_QM, outdated_node_id, target_node_id);
}

// test case tests forwarding of call of RegisterEventNotification() to
// NotifyEventHandler::RegisterEventNotification().
// Since we do NOT want to introduce a mock for the NotifyEventHandler owned by the MessagePassingFacade, because
// it would introduce polymorphism and injection APIs in the MessagePassingFacade only for testing this simple call-
// forwarding for coverage completeness, we just stimulate the simplest possible call and annotate a call expectation
// of a call done by SubscribeEventHandler. If this call happens, we are sure, that the call forwarding was correct.
TEST_F(MessagePassingFacadeFixture, RegisterEventNotification)
{
    safecpp::Scope<> event_receive_handler_scope{};
    auto eventUpdateNotificationHandler =
        std::make_shared<ScopedEventReceiveHandler>(event_receive_handler_scope, []() noexcept {
            return;
        });

    // we have a Facade created for QM only
    PrepareFacade(false);

    // expect, that GetNodeIdentifier() will get called (by the internal NotifyEventHandler owned bei UuT)
    EXPECT_CALL(message_passing_control_mock_, GetNodeIdentifier()).WillOnce(Return(OUR_PID));

    // when calling RegisterEventNotification, we will cover call forwarding to NotifyEventHandler
    unit_.value().RegisterEventNotification(
        QualityType::kASIL_QM, SOME_ELEMENT_FQ_ID, std::move(eventUpdateNotificationHandler), OUR_PID);
}

// test case tests forwarding of call of UnregisterEventNotification() to
// NotifyEventHandler::RegisterEventNotification().
// Since we do NOT want to introduce a mock for the NotifyEventHandler owned by the MessagePassingFacade, because
// it would introduce polymorphism and injection APIs in the MessagePassingFacade only for testing this simple call-
// forwarding for coverage completeness, we just stimulate the simplest possible call and annotate a call expectation
// of a call done by NotifyEventHandler. If this call happens, we are sure, that the call forwarding was correct.
TEST_F(MessagePassingFacadeFixture, UnregisterEventNotification)
{
    IMessagePassingService::HandlerRegistrationNoType invalid_registration_no = 7882;

    // we have a Facade created for QM only
    PrepareFacade(false);

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
