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
#include "score/mw/com/gateway/transport_layer/sample/sample_hypervisor_transport.h"

#include "score/mw/com/gateway/gateway_application/gateway_core_mock.h"
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport_mock.h"
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/log/recorder_mock.h"

#include <gtest/gtest.h>
#include <iostream>

namespace score::mw::com::gateway
{

namespace
{
class SampleHyperVisorTransportTest : public ::testing::Test
{
  public:
    SampleHyperVisorTransportTest() {}

    ~SampleHyperVisorTransportTest() override {}

    SampleHyperVisorTransportTest& WithASampleHyperVisorTransport()
    {
        transport_ = std::make_unique<SampleHyperVisorTransport>(gateway_core_mock_, std::move(mock_owner_));
        return *this;
    }

    SampleHyperVisorTransportTest& WithARegisteredOnSetupCallback()
    {
        // Capture the message handler callback set during Setup()

        EXPECT_CALL(*bi_directional_transport_mock_, SetMessageHandler(::testing::_))
            .WillOnce([this](IBidirectionalTransport::MessageHandler handler) {
                captured_handler_ = std::move(handler);
            });
        EXPECT_CALL(*bi_directional_transport_mock_, Setup()).WillOnce(::testing::Return(score::ResultBlank{}));
        const auto setup_result = transport_->Setup();
        EXPECT_TRUE(setup_result.has_value());

        return *this;
    }

    impl::InstanceSpecifier CreateValidInstanceSpecifier()
    {
        const auto specifier_result = impl::InstanceSpecifier::Create("SpeedService/Instance42");
        EXPECT_TRUE(specifier_result.has_value());
        return specifier_result.value();
    }

    std::unique_ptr<TransportMessage> CreateMessageOfType(MessageType type, bool valid_instance_specifier = true)
    {
        if (!valid_instance_specifier)
        {
            switch (type)
            {
                case MessageType::kProvideServiceRequest:
                    return std::make_unique<ProvideServiceRequest>();
                case MessageType::kStopOfferServiceRequest:
                    return std::make_unique<StopOfferServiceRequest>();
                case MessageType::kOfferServiceRequest:
                    return std::make_unique<OfferServiceRequest>();
                case MessageType::kRegisterNotificationRequest:
                    return std::make_unique<RegisterNotificationRequest>();
                case MessageType::kUnregisterNotificationRequest:
                    return std::make_unique<UnregisterNotificationRequest>();
                case MessageType::kUpdateNotification:
                    return std::make_unique<UpdateNotification>();
                case MessageType::kAckResponse:
                    return std::make_unique<AckResponse>();
                case MessageType::kInvalid:
                default:
                    return nullptr;
            }
        }
        impl::InstanceSpecifier specifier = CreateValidInstanceSpecifier();

        switch (type)
        {
            case MessageType::kProvideServiceRequest:
            {
                std::vector<impl::EventInfo> elements{};
                constexpr std::uint32_t kShmControlSize = 1024U;
                constexpr std::uint32_t kShmDataSize = 4096U;
                return std::make_unique<ProvideServiceRequest>(specifier, elements, kShmControlSize, kShmDataSize);
            }
            case MessageType::kStopOfferServiceRequest:
            {
                return std::make_unique<StopOfferServiceRequest>(specifier);
            }
            case MessageType::kOfferServiceRequest:
            {
                return std::make_unique<OfferServiceRequest>(specifier);
            }
            case MessageType::kRegisterNotificationRequest:
            {
                return std::make_unique<RegisterNotificationRequest>(
                    specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
            }
            case MessageType::kUnregisterNotificationRequest:
            {
                return std::make_unique<UnregisterNotificationRequest>(
                    specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
            }
            case MessageType::kUpdateNotification:
            {
                return std::make_unique<UpdateNotification>(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
            }
            case MessageType::kAckResponse:
                return std::make_unique<AckResponse>();
            case MessageType::kInvalid:
            default:
                return nullptr;
        }
    }

  protected:
    void SetUp() override
    {
        mock_owner_ = std::make_unique<BidirectionalTransportMock>();
        bi_directional_transport_mock_ = mock_owner_.get();
    }

    void TearDown() override {}

    std::unique_ptr<SampleHyperVisorTransport> transport_;
    // Raw pointer to the mock of BidirectionalTransport, either owned by mock_owner_ or by transport_
    BidirectionalTransportMock* bi_directional_transport_mock_{nullptr};
    // Owns the mock until it is moved into transport_ via WithASampleHyperVisorTransport()
    std::unique_ptr<BidirectionalTransportMock> mock_owner_;
    GatewayCoreMock gateway_core_mock_;
    IBidirectionalTransport::MessageHandler captured_handler_;
};

TEST_F(SampleHyperVisorTransportTest, CanBeConstructedWithValidConfiguration)
{
    EXPECT_NO_THROW(SampleHyperVisorTransport transport(gateway_core_mock_, std::move(mock_owner_)));
}

TEST_F(SampleHyperVisorTransportTest, IsMemorySharingSupportedReturnsTrue)
{
    SampleHyperVisorTransport transport(gateway_core_mock_, std::move(mock_owner_));
    EXPECT_TRUE(transport.IsMemorySharingSupported());
}

TEST_F(SampleHyperVisorTransportTest, SetupCallsSetMessageHandlerAndSetupOnTransport)
{
    EXPECT_CALL(*bi_directional_transport_mock_, SetMessageHandler(::testing::_)).Times(1);
    EXPECT_CALL(*bi_directional_transport_mock_, Setup()).WillOnce(::testing::Return(score::ResultBlank{}));

    SampleHyperVisorTransport transport(gateway_core_mock_, std::move(mock_owner_));
    const auto result = transport.Setup();
    EXPECT_TRUE(result.has_value());
}

TEST_F(SampleHyperVisorTransportTest, SetupReturnsErrorWhenTransportSetupFails)
{
    // Given a SampleHyperVisorTransport with a mocked BidirectionalTransport
    this->WithASampleHyperVisorTransport();
    // when the BidirectionalTransport's Setup method returns a connection failure error

    EXPECT_CALL(*bi_directional_transport_mock_, SetMessageHandler(::testing::_)).Times(1);
    EXPECT_CALL(*bi_directional_transport_mock_, Setup())
        .WillOnce(::testing::Return(score::MakeUnexpected(TransportErrorc::kConnectionFailure)));
    const auto result = transport_->Setup();
    // then calling Setup on SampleHyperVisorTransport should return an error
    EXPECT_FALSE(result.has_value());
}

TEST_F(SampleHyperVisorTransportTest, ShutdownCallsShutdownOnTransport)
{
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(2);

    SampleHyperVisorTransport transport(gateway_core_mock_, std::move(mock_owner_));
    transport.Shutdown();
}

TEST_F(SampleHyperVisorTransportTest, OfferServiceRequestWithCorrectType)
{
    // Given a SampleHyperVisorTransport with a mocked BidirectionalTransport and a valid instance specifier
    this->WithASampleHyperVisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();
    // then the BidirectionalTransport's SendRequest method should be called with a TransportMessage of type
    // kOfferServiceRequest
    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kOfferServiceRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    // when calling OfferService on SampleHyperVisorTransport
    transport_->OfferService(specifier);
}

TEST_F(SampleHyperVisorTransportTest, StopOfferServiceRequestWithCorrectType)
{
    // Given a SampleHyperVisorTransport with a mocked BidirectionalTransport and a valid instance specifier
    this->WithASampleHyperVisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();
    // then the BidirectionalTransport's SendRequest method should be called with a TransportMessage of type
    // kStopOfferServiceRequest
    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kStopOfferServiceRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    // when calling StopOfferService on SampleHyperVisorTransport
    transport_->StopOfferService(specifier);
}

TEST_F(SampleHyperVisorTransportTest, NotifyUpdateWithCorrectType)
{
    // Given a SampleHyperVisorTransport with a mocked BidirectionalTransport and a valid instance specifier
    this->WithASampleHyperVisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();
    // then the BidirectionalTransport's SendNotification method should be called with a TransportMessage of type
    // kUpdateNotification
    EXPECT_CALL(*bi_directional_transport_mock_,
                SendNotification(::testing::Property(&TransportMessage::GetType, MessageType::kUpdateNotification)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    // when calling NotifyUpdate on SampleHyperVisorTransport
    transport_->NotifyUpdate(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
}

TEST_F(SampleHyperVisorTransportTest, RegisterUpdateNotificationWithCorrectType)
{
    // Given a SampleHyperVisorTransport with a mocked BidirectionalTransport and a valid instance specifier
    this->WithASampleHyperVisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();
    // then the BidirectionalTransport's SendRequest method should be called with a TransportMessage of type
    // kRegisterNotificationRequest
    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kRegisterNotificationRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    // when calling RegisterUpdateNotification on SampleHyperVisorTransport
    transport_->RegisterUpdateNotification(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
}

TEST_F(SampleHyperVisorTransportTest, UnregisterUpdateNotificationWithCorrectType)
{
    // Given a SampleHyperVisorTransport with a mocked BidirectionalTransport and a valid instance specifier
    this->WithASampleHyperVisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();
    // then the BidirectionalTransport's SendRequest method should be called with a TransportMessage of type
    // kUnregisterNotificationRequest
    EXPECT_CALL(
        *bi_directional_transport_mock_,
        SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kUnregisterNotificationRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    // when calling UnregisterUpdateNotification on SampleHyperVisorTransport
    transport_->UnregisterUpdateNotification(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
}

TEST_F(SampleHyperVisorTransportTest, ResolveShmPathReturnsEmptyObjectIfSpecifierCanNotBeResolved)
{
    // Given a valid instance specifier and a mocked runtime
    const auto specifier = CreateValidInstanceSpecifier();
    impl::RuntimeMockGuard runtime_mock_guard_{};

    // When the runtime can not resolve the instance specifier
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(::testing::_))
        .WillOnce(::testing::Return(std::vector<impl::InstanceIdentifier>{}));

    // Then the returned SHM paths for control and data should both be empty
    const auto resolved_shm_paths = ResolveShmPaths(specifier);
    EXPECT_TRUE(resolved_shm_paths.control.empty());
    EXPECT_TRUE(resolved_shm_paths.data.empty());
}

TEST_F(SampleHyperVisorTransportTest, ResolveShmPathReturnsEmptyObjectIfServiceInstanceDeploymentIsInvalid)
{
    // Given a valid instance specifier and a mocked runtime
    const auto specifier = CreateValidInstanceSpecifier();
    impl::RuntimeMockGuard runtime_mock_guard_{};

    // When providing an (invalid) instance identifier without a instance deployment that will be used to resolve the
    // SHM paths
    score::mw::com::impl::DummyInstanceIdentifierBuilder builder{};
    auto instance_identifier = builder.CreateBlankBindingInstanceIdentifier();

    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(::testing::_))
        .WillOnce(::testing::Return(std::vector<impl::InstanceIdentifier>{instance_identifier}));

    // Then the returned SHM paths for control and data should both be empty
    const auto resolved_shm_paths = ResolveShmPaths(specifier);
    EXPECT_TRUE(resolved_shm_paths.control.empty());
    EXPECT_TRUE(resolved_shm_paths.data.empty());
}

TEST_F(SampleHyperVisorTransportTest, ResolveShmPathReturnsEmptyObjectIfServiceTypeDeploymentIsInvalid)
{
    // Given a valid instance specifier and a mocked runtime
    const auto specifier = CreateValidInstanceSpecifier();
    impl::RuntimeMockGuard runtime_mock_guard_{};

    // When providing an (invalid) instance identifier without a type deployment that will be used to resolve the SHM
    // paths
    score::mw::com::impl::DummyInstanceIdentifierBuilder builder{};
    auto instance_identifier = builder.CreateLolaInstanceIdentifierWithoutTypeDeployment();

    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(::testing::_))
        .WillOnce(::testing::Return(std::vector<impl::InstanceIdentifier>{instance_identifier}));

    // Then the returned SHM paths for control and data should both be empty
    const auto resolved_shm_paths = ResolveShmPaths(specifier);
    EXPECT_TRUE(resolved_shm_paths.control.empty());
    EXPECT_TRUE(resolved_shm_paths.data.empty());
}

TEST_F(SampleHyperVisorTransportTest, GetShmSizesReturnsZeroSizesIfResolveShmPathsReturnsEmptyPaths)
{
    // Given a valid instance specifier and a mocked runtime
    const auto specifier = CreateValidInstanceSpecifier();
    impl::RuntimeMockGuard runtime_mock_guard_{};

    // When providing an (invalid) instance identifier without a type deployment that will be used to resolve the SHM
    // paths
    score::mw::com::impl::DummyInstanceIdentifierBuilder builder{};
    auto instance_identifier = builder.CreateLolaInstanceIdentifierWithoutTypeDeployment();

    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(::testing::_))
        .WillOnce(::testing::Return(std::vector<impl::InstanceIdentifier>{instance_identifier}));

    // Then the returned SHM sizes for control and data should both be 0
    const auto shm_sizes = GetShmSizes(specifier);
    EXPECT_TRUE(shm_sizes.control == 0U);
    EXPECT_TRUE(shm_sizes.data == 0U);
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedProvideServiceRequestWithValidInstanceSpecifierCallsProvideServiceOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();
    impl::RuntimeMockGuard runtime_mock_guard_{};
    // PreCreateInterVmSharedMemory calls ResolveShmPaths, which needs the runtime mock.
    // Return empty identifiers so that ResolveShmPaths returns empty paths and PreCreateInterVmSharedMemory returns
    // early.
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(::testing::_))
        .WillOnce(::testing::Return(std::vector<impl::InstanceIdentifier>{}));

    // When a ProvideServiceRequest message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);

    // Then ProvideService should be called on the gateway core with the matching instance specifier and service
    // elements
    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedProvideServiceRequestWithInvalidInstanceSpecifierReturnsWithoutCoreCall)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();
    impl::RuntimeMockGuard runtime_mock_guard_{};

    // When a ProvideServiceRequest message with an invalid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest, false);

    // Then ProvideService should not be called
    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_)).Times(0);

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedStopOfferServiceRequestWithValidInstanceSpecifierCallsStopOfferServiceOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When a StopOfferServiceRequest message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kStopOfferServiceRequest);

    // Then StopOfferService should be called on the gateway core with the matching instance specifier
    EXPECT_CALL(gateway_core_mock_, StopOfferService(::testing::_)).WillOnce(::testing::Return());
    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedStopOfferServiceRequestWithInvalidInstanceSpecifierReturnsWithoutCoreCall)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When a StopOfferServiceRequest message with an invalid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kStopOfferServiceRequest, false);

    // Then StopOfferService should be not be called on the gateway core because it will return early
    EXPECT_CALL(gateway_core_mock_, StopOfferService(::testing::_)).Times(0);
    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedOfferServiceRequestWithValidInstanceSpecifierCallsOfferServiceOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an OfferServiceRequest message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kOfferServiceRequest);

    // Then OfferService should be called on the gateway core with the matching instance specifier
    EXPECT_CALL(gateway_core_mock_, OfferService(::testing::_)).WillOnce(::testing::Return(score::ResultBlank{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedOfferServiceRequestWithValidInstanceSpecifierReturnsWithoutCoreCall)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an OfferServiceRequest message with an invalid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kOfferServiceRequest, false);

    // Then OfferService should not be called on the gateway core because it will return early due to invalid instance
    // specifier
    EXPECT_CALL(gateway_core_mock_, OfferService(::testing::_)).Times(0);

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(
    SampleHyperVisorTransportTest,
    OnMessageReceivedRegisterNotificationRequestWithValidInstanceSpecifierCallsRegisterNotificationRequestOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an RegisterNotificationRequest message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kRegisterNotificationRequest);

    // Then RegisterNotification should be called on the gateway core with the matching instance specifier
    EXPECT_CALL(gateway_core_mock_, RegisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedRegisterNotificationRequestWithInvalidInstanceSpecifierReturnsWithoutCoreCall)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an RegisterNotificationRequest message with an invalid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kRegisterNotificationRequest, false);

    // Then RegisterNotification should not be called on the gateway core because it wil return early
    EXPECT_CALL(gateway_core_mock_, RegisterUpdateNotification(::testing::_, ::testing::_, ::testing::_)).Times(0);

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(
    SampleHyperVisorTransportTest,
    OnMessageReceivedUnregisterNotificationRequestWithValidInstanceSpecifierCallsUnregisterNotificationRequestOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an RegisterNotificationRequest message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kUnregisterNotificationRequest);

    // Then UnregisterNotification should be called on the gateway core with the matching instance specifier
    EXPECT_CALL(gateway_core_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedUnregisterNotificationRequestWithInvalidInstanceSpecifierReturnsWithoutCoreCall)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an RegisterNotificationRequest message with an invalid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kUnregisterNotificationRequest, false);

    // Then UnregisterNotification should not be called on the gateway core because it will return early
    EXPECT_CALL(gateway_core_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_)).Times(0);

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(
    SampleHyperVisorTransportTest,
    OnMessageReceivedUnregisterNotificationRequestWithValidInstanceSpecifierCallsUnregisterUpdateNotificationOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an RegisterNotificationRequest message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kUnregisterNotificationRequest);

    // Then UnregisterNotification should be called on the gateway core with the matching instance specifier
    EXPECT_CALL(gateway_core_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedUpdateNotificationWithValidInstanceSpecifierCallsNotifyUpdateOnGatewayCore)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an UpdateNotification message with a valid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kUpdateNotification);

    // Then NotifyUpdate should be called on the gateway core with the matching instance specifier
    EXPECT_CALL(gateway_core_mock_, NotifyUpdate(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest,
       OnMessageReceivedUpdateNotificationWithInvalidInstanceSpecifierReturnsWithoutCoreCall)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();

    // When an UpdateNotification message with an invalid instance specifier is received
    auto request = CreateMessageOfType(MessageType::kUpdateNotification, false);

    // Then NotifyUpdate should not be called on the gateway core
    EXPECT_CALL(gateway_core_mock_, NotifyUpdate(::testing::_, ::testing::_, ::testing::_)).Times(0);

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest, UnsupportedMessageWillBeLogged)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();
    // and a mocked LogRecorder
    score::mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);

    // When a message with an unsupported type is received, i.e. not considered by the Sample transport
    auto request = CreateMessageOfType(MessageType::kAckResponse);

    // Then an error should be logged
    EXPECT_CALL(recorder_mock, StartRecord(::testing::_, mw::log::LogLevel::kError))
        .WillOnce(::testing::Return(mw::log::SlotHandle{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest, InvalidMessageWillBeLogged)
{
    // Given a SampleHyperVisorTransport, a mocked runtime and a message handler callback has been set for the
    // underlying BiDirectionalTransport
    this->WithASampleHyperVisorTransport().WithARegisteredOnSetupCallback();
    // and a mocked LogRecorder
    score::mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);

    // When an invalid message is received (MessageType::kInvalid will cause CreateMessageOfType to return a nullptr)
    auto request = CreateMessageOfType(MessageType::kInvalid);

    // Then an error should be logged
    EXPECT_CALL(recorder_mock, StartRecord(::testing::_, mw::log::LogLevel::kError))
        .WillOnce(::testing::Return(mw::log::SlotHandle{}));

    // Invoke the captured handler to trigger OnMessageReceived
    captured_handler_(std::move(request));
}

TEST_F(SampleHyperVisorTransportTest, ProvideServiceDeathTest)
{
    // Given a SampleHyperVisorTransport
    this->WithASampleHyperVisorTransport();

    // When calling ProvideService with a valid instance specifier, then it is expected to terminate.
    // TODO This test needs to be adapted when implementing ResolveShmPaths() and GetShmSizes() based on the actual
    // HyperVisor SHM technology.
    EXPECT_DEATH(transport_->ProvideService(CreateValidInstanceSpecifier(), std::vector<impl::EventInfo>{}), ".*");
}

}  // namespace

}  // namespace score::mw::com::gateway
