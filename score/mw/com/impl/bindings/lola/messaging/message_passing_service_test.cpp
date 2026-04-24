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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_factory_mock.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_mock.h"
#include "score/mw/com/impl/com_error.h"

#include <gmock/gmock.h>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

#include <memory>

namespace score::mw::com::impl::lola::test
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

MATCHER_P(MatchesAsilSpecificConfig, cfg, "")
{
    return arg.message_queue_rx_size_ == cfg.message_queue_rx_size_ && arg.allowed_user_ids_ == cfg.allowed_user_ids_;
}

class MessagePassingServiceTest : public ::testing::Test
{
  protected:
    MessagePassingServiceTest& WithAsilBAndQmInstance()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG_MESSAGE(asil_qm_message_passing_service_instance_mock_ != nullptr &&
                                                        asil_b_message_passing_service_instance_mock_ != nullptr,
                                                    "Dependencies invalid");

        ON_CALL(*factory_, Create(ClientQualityType::kASIL_B, MatchesAsilSpecificConfig(asil_b_cfg_), _, _, _))
            .WillByDefault(Return(ByMove(std::move(asil_b_message_passing_service_instance_mock_))));
        ON_CALL(*factory_, Create(ClientQualityType::kASIL_QMfromB, MatchesAsilSpecificConfig(asil_qm_cfg_), _, _, _))
            .WillByDefault(Return(ByMove(std::move(asil_qm_message_passing_service_instance_mock_))));
        return *this;
    }

    MessagePassingServiceTest& WithAsilQmInstance()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG_MESSAGE(asil_qm_message_passing_service_instance_mock_ != nullptr,
                                                    "Dependencies invalid");

        ON_CALL(*factory_, Create(ClientQualityType::kASIL_QM, MatchesAsilSpecificConfig(asil_qm_cfg_), _, _, _))
            .WillByDefault(Return(ByMove(std::move(asil_qm_message_passing_service_instance_mock_))));

        return *this;
    }

    AsilSpecificCfg asil_qm_cfg_{1, {}};
    AsilSpecificCfg asil_b_cfg_{1, {}};
    std::unique_ptr<MessagePassingServiceInstanceMock> asil_b_message_passing_service_instance_mock_ =
        std::make_unique<MessagePassingServiceInstanceMock>();
    std::unique_ptr<MessagePassingServiceInstanceMock> asil_qm_message_passing_service_instance_mock_ =
        std::make_unique<MessagePassingServiceInstanceMock>();

    std::unique_ptr<MessagePassingServiceInstanceFactoryMock> factory_ =
        std::make_unique<MessagePassingServiceInstanceFactoryMock>();
};

TEST_F(MessagePassingServiceTest, CreatesQMInstanceForQMOnlyService)
{
    // Given a unit with no dependencies to inject
    WithAsilQmInstance();

    // Expecting a construction of an ASIL-QM instance and none for an ASIL-B instance
    EXPECT_CALL(*factory_, Create(ClientQualityType::kASIL_B, _, _, _, _)).Times(0);
    EXPECT_CALL(*factory_, Create(ClientQualityType::kASIL_QM, MatchesAsilSpecificConfig(asil_qm_cfg_), _, _, _))
        .Times(1);

    // When constructing the unit
    const MessagePassingService unit{asil_qm_cfg_, std::nullopt, std::move(factory_)};
}

TEST_F(MessagePassingServiceTest, NotifyEventDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to NotifyEvent of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyEvent(event_id)).Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyEvent(_)).Times(0);

    // When calling NotifyEvent
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyEvent(QualityType::kASIL_QM, event_id);
}

TEST_F(MessagePassingServiceTest, NotifyEventDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to NotifyEvent of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyEvent(_)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyEvent(event_id)).Times(1);

    // When calling NotifyEvent
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyEvent(QualityType::kASIL_B, event_id);
}

TEST_F(MessagePassingServiceTest, DeathTestNotifyEventAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // When calling NotifyEvent with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};

    // Then we expect it to have died
    EXPECT_DEATH(unit.NotifyEvent(QualityType::kInvalid, event_id), "");
}

MATCHER_P(MatchesWeakPtr, ptr, "")
{
    return arg.lock() == ptr.lock();
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    const auto callback = std::make_shared<ScopedEventReceiveHandler>();
    const std::weak_ptr<ScopedEventReceiveHandler> weak_callback = callback;
    constexpr pid_t pid{5};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterEventNotification(event_id, MatchesWeakPtr(weak_callback), pid))
        .WillOnce(Return(handler_no));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterEventNotification(_, _, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    const auto result = unit.RegisterEventNotification(QualityType::kASIL_QM, event_id, weak_callback, pid);

    // Then the return value matches the injected handler_no
    EXPECT_EQ(result, handler_no);
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    const auto callback = std::make_shared<ScopedEventReceiveHandler>();
    const std::weak_ptr<ScopedEventReceiveHandler> weak_callback = callback;
    constexpr pid_t pid{5};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, RegisterEventNotification(_, _, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                RegisterEventNotification(event_id, MatchesWeakPtr(weak_callback), pid))
        .WillOnce(Return(handler_no));

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    const auto result = unit.RegisterEventNotification(QualityType::kASIL_B, event_id, weak_callback, pid);

    // Then the return value matches the injected handler_no
    EXPECT_EQ(result, handler_no);
}

using MessagePassingServiceDeathTest = MessagePassingServiceTest;
TEST_F(MessagePassingServiceDeathTest, RegisterEventNotificationAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    const auto callback = std::make_shared<ScopedEventReceiveHandler>();
    const std::weak_ptr<ScopedEventReceiveHandler> weak_callback = callback;
    constexpr pid_t pid{5};

    // When calling RegisterEventNotification with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.RegisterEventNotification(QualityType::kInvalid, event_id, weak_callback, pid), "");
}

TEST_F(MessagePassingServiceTest, ReregisterEventNotificationDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, ReregisterEventNotification(event_id, pid)).Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, ReregisterEventNotification(_, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.ReregisterEventNotification(QualityType::kASIL_QM, event_id, pid);
}

TEST_F(MessagePassingServiceTest, ReregisterEventNotificationDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, ReregisterEventNotification(_, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, ReregisterEventNotification(event_id, pid)).Times(1);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.ReregisterEventNotification(QualityType::kASIL_B, event_id, pid);
}

TEST_F(MessagePassingServiceDeathTest, ReregisterEventNotificationAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr pid_t pid{5};

    // When calling ReregisterEventNotification with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.ReregisterEventNotification(QualityType::kInvalid, event_id, pid), "");
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterEventNotification(event_id, handler_no, pid))
        .Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterEventNotification(_, _, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotification(QualityType::kASIL_QM, event_id, handler_no, pid);
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterEventNotification(_, _, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterEventNotification(event_id, handler_no, pid))
        .Times(1);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotification(QualityType::kASIL_B, event_id, handler_no, pid);
}

TEST_F(MessagePassingServiceDeathTest, UnregisterEventNotificationAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};
    constexpr pid_t pid{5};

    // When calling UnregisterEventNotification with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.UnregisterEventNotification(QualityType::kInvalid, event_id, handler_no, pid), "");
}

TEST_F(MessagePassingServiceTest, NotifyOutdatedNodeIdDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    constexpr pid_t pid{5};
    constexpr pid_t old_pid{4};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyOutdatedNodeId(pid, old_pid)).Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyOutdatedNodeId(_, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyOutdatedNodeId(QualityType::kASIL_QM, pid, old_pid);
}

TEST_F(MessagePassingServiceTest, NotifyOutdatedNodeIdDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    constexpr pid_t pid{5};
    constexpr pid_t old_pid{4};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyOutdatedNodeId(_, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyOutdatedNodeId(pid, old_pid)).Times(1);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyOutdatedNodeId(QualityType::kASIL_B, pid, old_pid);
}

TEST_F(MessagePassingServiceDeathTest, NotifyOutdatedNodeIdAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    constexpr pid_t pid{5};
    constexpr pid_t old_pid{4};

    // When calling NotifyOutdatedNodeId with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.NotifyOutdatedNodeId(QualityType::kInvalid, pid, old_pid), "");
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationExistenceChangedCallbackDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to RegisterEventNotificationExistenceChangedCallback of ASIL-B mock instance
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                RegisterEventNotificationExistenceChangedCallback(event_id, _))
        .Times(1);
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterEventNotificationExistenceChangedCallback(_, _))
        .Times(0);

    // When calling RegisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.RegisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, event_id, {});
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationExistenceChangedCallbackDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to RegisterEventNotificationExistenceChangedCallback of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterEventNotificationExistenceChangedCallback(event_id, _))
        .Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterEventNotificationExistenceChangedCallback(_, _))
        .Times(0);

    // When calling RegisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.RegisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, event_id, {});
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationExistenceChangedCallbackDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to UnregisterEventNotificationExistenceChangedCallback of ASIL-B mock instance
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                UnregisterEventNotificationExistenceChangedCallback(event_id))
        .Times(1);
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterEventNotificationExistenceChangedCallback(_))
        .Times(0);

    // When calling UnregisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, event_id);
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationExistenceChangedCallbackDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to UnregisterEventNotificationExistenceChangedCallback of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                UnregisterEventNotificationExistenceChangedCallback(event_id))
        .Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterEventNotificationExistenceChangedCallback(_))
        .Times(0);

    // When calling UnregisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, event_id);
}
class MessagePassingServiceQMDelegationTest : public MessagePassingServiceTest
{
  protected:
    MessagePassingService& GivenAMessagePassingServiceWithAsilBAndQm()
    {
        WithAsilBAndQmInstance();
        unit_ = std::make_unique<MessagePassingService>(asil_qm_cfg_, asil_qm_cfg_, std::move(factory_));
        return *unit_;
    }

    const ProxyInstanceIdentifier kProxyInstanceId{5U, 1U};
    const ProxyMethodInstanceIdentifier kProxyMethodInstanceId{kProxyInstanceId, LolaMethodId{55U}};
    const SkeletonInstanceIdentifier kSkeletonInstanceId{LolaServiceId{12U}, LolaServiceInstanceId::InstanceId{22U}};
    const pid_t kTargetNodeId{7};
    const uid_t kAllowedUid{42U};
    const std::size_t kQueuePosition{2U};

  private:
    std::unique_ptr<MessagePassingService> unit_;
};

TEST_F(MessagePassingServiceQMDelegationTest, RegisterOnServiceMethodSubscribedHandlerCallReturnsAValue)
{
    // Given some input parameters to the tested function call
    IMessagePassingService::ServiceMethodSubscribedHandler callback;
    IMessagePassingService::AllowedConsumerUids allowed_uids;

    // Expecting a call to RegisterOnServiceMethodSubscribedHandler of ASIL-QM mock instance and no call to the asil-b
    // instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterOnServiceMethodSubscribedHandler(kSkeletonInstanceId, _, allowed_uids))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterOnServiceMethodSubscribedHandler(_, _, _))
        .Times(0);

    // When calling RegisterOnServiceMethodSubscribedHandler  with `QualityType::kASIL_QM`
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().RegisterOnServiceMethodSubscribedHandler(
        QualityType::kASIL_QM, kSkeletonInstanceId, std::move(callback), allowed_uids);

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, RegisterMethodCallHandlerCallReturnsAValue)
{
    // Given some input parameters to the tested function call
    IMessagePassingService::MethodCallHandler callback;

    // Expecting a call to RegisterMethodCallHandler of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterMethodCallHandler(kProxyMethodInstanceId, _, kAllowedUid))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterMethodCallHandler(_, _, _)).Times(0);

    // When calling RegisterMethodCallHandler
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().RegisterMethodCallHandler(
        QualityType::kASIL_QM, kProxyMethodInstanceId, std::move(callback), kAllowedUid);

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, SubscribeServiceMethodReturnsAValue)
{
    // Expecting a call to SubscribeServiceMethod of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                SubscribeServiceMethod(kSkeletonInstanceId, kProxyInstanceId, kTargetNodeId))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, SubscribeServiceMethod(_, _, _)).Times(0);

    // When calling SubscribeServiceMethod
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().SubscribeServiceMethod(
        QualityType::kASIL_QM, kSkeletonInstanceId, kProxyInstanceId, kTargetNodeId);

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, CallMethodReturnsAValue)
{
    // Expecting a call to CallMethod of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                CallMethod(kProxyMethodInstanceId, kQueuePosition, kTargetNodeId))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, CallMethod(_, _, _)).Times(0);

    // When calling CallMethod
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().CallMethod(
        QualityType::kASIL_QM, kProxyMethodInstanceId, kQueuePosition, kTargetNodeId);

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, RegisterOnServiceMethodUnsubscribedHandlerDispatchesToQmInstance)
{
    // Given some input parameters to the tested function call
    IMessagePassingService::ServiceMethodUnsubscribedHandler callback;

    // Expecting a call to RegisterOnServiceMethodUnsubscribedHandler of ASIL-QM mock instance and no call to ASIL-B
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId, _))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterOnServiceMethodUnsubscribedHandler(_, _))
        .Times(0);

    // When calling RegisterOnServiceMethodUnsubscribedHandler with QualityType::kASIL_QM
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().RegisterOnServiceMethodUnsubscribedHandler(
        QualityType::kASIL_QM, kSkeletonInstanceId, std::move(callback));

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, RegisterOnServiceMethodUnsubscribedHandlerDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    IMessagePassingService::ServiceMethodUnsubscribedHandler callback;

    // Expecting a call to RegisterOnServiceMethodUnsubscribedHandler of ASIL-B mock instance and no call to QM
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                RegisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId, _))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, RegisterOnServiceMethodUnsubscribedHandler(_, _))
        .Times(0);

    // When calling RegisterOnServiceMethodUnsubscribedHandler with QualityType::kASIL_B
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().RegisterOnServiceMethodUnsubscribedHandler(
        QualityType::kASIL_B, kSkeletonInstanceId, std::move(callback));

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest,
       RegisterOnServiceMethodUnsubscribedHandlerReturnsErrorWhenInstanceRegistrationFails)
{
    // Given some input parameters to the tested function call
    IMessagePassingService::ServiceMethodUnsubscribedHandler callback;

    // Expecting a call to RegisterOnServiceMethodUnsubscribedHandler of ASIL-QM mock instance which returns an error
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId, _))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    // When calling RegisterOnServiceMethodUnsubscribedHandler
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().RegisterOnServiceMethodUnsubscribedHandler(
        QualityType::kASIL_QM, kSkeletonInstanceId, std::move(callback));

    // Then the result should be an error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceQMDelegationTest, UnsubscribeServiceMethodDispatchesToQmInstance)
{
    // Expecting a call to UnsubscribeServiceMethod of ASIL-QM mock instance and no call to ASIL-B
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                UnsubscribeServiceMethod(kSkeletonInstanceId, kProxyInstanceId, kTargetNodeId))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnsubscribeServiceMethod(_, _, _)).Times(0);

    // When calling UnsubscribeServiceMethod with QualityType::kASIL_QM
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().UnsubscribeServiceMethod(
        QualityType::kASIL_QM, kSkeletonInstanceId, kProxyInstanceId, kTargetNodeId);

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, UnsubscribeServiceMethodDispatchesToAsilBInstance)
{
    // Expecting a call to UnsubscribeServiceMethod of ASIL-B mock instance and no call to QM
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                UnsubscribeServiceMethod(kSkeletonInstanceId, kProxyInstanceId, kTargetNodeId))
        .WillOnce(Return(score::ResultBlank{}));
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnsubscribeServiceMethod(_, _, _)).Times(0);

    // When calling UnsubscribeServiceMethod with QualityType::kASIL_B
    const auto result = GivenAMessagePassingServiceWithAsilBAndQm().UnsubscribeServiceMethod(
        QualityType::kASIL_B, kSkeletonInstanceId, kProxyInstanceId, kTargetNodeId);

    // Then the result should have a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceQMDelegationTest, UnregisterOnServiceMethodUnsubscribedHandlerDispatchesToQmInstance)
{
    // Expecting a call to RegisterOnServiceMethodUnsubscribedHandler of ASIL-QM mock instance (to set it up)
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId, _))
        .WillOnce(Return(score::ResultBlank{}));

    // and expecting that UnregisterOnServiceMethodUnsubscribedHandler is called on ASIL-QM (not ASIL-B) when the guard
    // is destroyed
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                UnregisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterOnServiceMethodUnsubscribedHandler(_))
        .Times(0);

    // When RegisterOnServiceMethodUnsubscribedHandler is called and the returned guard is destroyed
    {
        const auto guard = GivenAMessagePassingServiceWithAsilBAndQm().RegisterOnServiceMethodUnsubscribedHandler(
            QualityType::kASIL_QM, kSkeletonInstanceId, {});
        ASSERT_TRUE(guard.has_value());
    }  // guard destroyed here — triggers unregister
}

TEST_F(MessagePassingServiceQMDelegationTest, UnregisterOnServiceMethodUnsubscribedHandlerDispatchesToAsilBInstance)
{
    // Expecting a call to RegisterOnServiceMethodUnsubscribedHandler of ASIL-B mock instance (to set it up)
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                RegisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId, _))
        .WillOnce(Return(score::ResultBlank{}));

    // and expecting that UnregisterOnServiceMethodUnsubscribedHandler is called on ASIL-B (not QM) when the guard
    // is destroyed
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                UnregisterOnServiceMethodUnsubscribedHandler(kSkeletonInstanceId));
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterOnServiceMethodUnsubscribedHandler(_))
        .Times(0);

    // When RegisterOnServiceMethodUnsubscribedHandler is called and the returned guard is destroyed
    {
        const auto guard = GivenAMessagePassingServiceWithAsilBAndQm().RegisterOnServiceMethodUnsubscribedHandler(
            QualityType::kASIL_B, kSkeletonInstanceId, {});
        ASSERT_TRUE(guard.has_value());
    }  // guard destroyed here — triggers unregister
}

}  // namespace score::mw::com::impl::lola::test
