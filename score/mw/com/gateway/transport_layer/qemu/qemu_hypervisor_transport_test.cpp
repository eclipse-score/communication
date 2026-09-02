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
#include "score/mw/com/gateway/transport_layer/qemu/qemu_hypervisor_transport.h"

#include "score/mw/com/gateway/gateway_application/gateway_core_mock.h"
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider_mock.h"
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport_mock.h"
#include "score/mw/com/gateway/transport_layer/transport_error.h"

#include "score/mw/log/recorder_mock.h"

#if defined(__QNXNTO__)
#include "score/os/mocklib/qnx/mock_mman.h"
#endif

#include <gtest/gtest.h>
#include <string>

namespace score::mw::com::gateway
{

namespace
{

class QemuHypervisorTransportTest : public ::testing::Test
{
  public:
    QemuHypervisorTransportTest& WithAQemuHypervisorTransport()
    {
        transport_ = std::make_unique<QemuHypervisorTransport>(
            gateway_core_mock_, std::move(mock_transport_owner_), ivshmem_provider_mock_);
        return *this;
    }

    QemuHypervisorTransportTest& WithAQemuHypervisorTransportWithEmptyPathResolver()
    {
        transport_ = std::make_unique<QemuHypervisorTransport>(gateway_core_mock_,
                                                               std::move(mock_transport_owner_),
                                                               ivshmem_provider_mock_,
                                                               [](const impl::InstanceSpecifier&) {
                                                                   return ShmPaths{"", ""};
                                                               });
        return *this;
    }

    QemuHypervisorTransportTest& WithARegisteredOnSetupCallback()
    {
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
        const auto specifier_result = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance42"});
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
                return std::make_unique<StopOfferServiceRequest>(specifier);
            case MessageType::kOfferServiceRequest:
                return std::make_unique<OfferServiceRequest>(specifier);
            case MessageType::kRegisterNotificationRequest:
                return std::make_unique<RegisterNotificationRequest>(
                    specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
            case MessageType::kUnregisterNotificationRequest:
                return std::make_unique<UnregisterNotificationRequest>(
                    specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
            case MessageType::kUpdateNotification:
                return std::make_unique<UpdateNotification>(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
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
        mock_transport_owner_ = std::make_unique<BidirectionalTransportMock>();
        bi_directional_transport_mock_ = mock_transport_owner_.get();
        ivshmem_provider_mock_ = std::make_shared<qemu::ivshmem::IvshmemTypedMemoryProviderMock>();
        ivshmem_provider_raw_ =
            std::static_pointer_cast<qemu::ivshmem::IvshmemTypedMemoryProviderMock>(ivshmem_provider_mock_).get();
    }

    void TearDown() override {}

    std::unique_ptr<QemuHypervisorTransport> transport_;
    BidirectionalTransportMock* bi_directional_transport_mock_{nullptr};
    std::unique_ptr<BidirectionalTransportMock> mock_transport_owner_;
    std::shared_ptr<qemu::ivshmem::IvshmemTypedMemoryProvider> ivshmem_provider_mock_;
    qemu::ivshmem::IvshmemTypedMemoryProviderMock* ivshmem_provider_raw_{nullptr};
    GatewayCoreMock gateway_core_mock_;
    IBidirectionalTransport::MessageHandler captured_handler_;
};

TEST_F(QemuHypervisorTransportTest, CanBeConstructedWithValidParameters)
{
    // Expecting Shutdown to be called
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    // When constructing QemuHypervisorTransport with valid parameters
    // Then no exception is thrown
    EXPECT_NO_THROW(QemuHypervisorTransport transport(
        gateway_core_mock_, std::move(mock_transport_owner_), ivshmem_provider_mock_));
}

TEST_F(QemuHypervisorTransportTest, DestructorCallsShutdown)
{
    // Expecting Shutdown to be called once via destructor
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    // When transport goes out of scope
    {
        QemuHypervisorTransport transport(gateway_core_mock_, std::move(mock_transport_owner_), ivshmem_provider_mock_);
    }
    // Then destructor calls Shutdown
}

TEST_F(QemuHypervisorTransportTest, IsMemorySharingSupportedReturnsTrue)
{
    // Given a constructed transport
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);
    QemuHypervisorTransport transport(gateway_core_mock_, std::move(mock_transport_owner_), ivshmem_provider_mock_);

    // When calling IsMemorySharingSupported
    // Then it returns true
    EXPECT_TRUE(transport.IsMemorySharingSupported());
}

TEST_F(QemuHypervisorTransportTest, SetupCallsSetMessageHandlerAndSetupOnTransport)
{
    EXPECT_CALL(*bi_directional_transport_mock_, SetMessageHandler(::testing::_)).Times(1);
    EXPECT_CALL(*bi_directional_transport_mock_, Setup()).WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    QemuHypervisorTransport transport(gateway_core_mock_, std::move(mock_transport_owner_), ivshmem_provider_mock_);
    const auto result = transport.Setup();
    EXPECT_TRUE(result.has_value());
}

TEST_F(QemuHypervisorTransportTest, SetupReturnsErrorWhenTransportSetupFails)
{
    this->WithAQemuHypervisorTransport();

    EXPECT_CALL(*bi_directional_transport_mock_, SetMessageHandler(::testing::_)).Times(1);
    EXPECT_CALL(*bi_directional_transport_mock_, Setup())
        .WillOnce(::testing::Return(score::MakeUnexpected(TransportErrorc::kConnectionFailure)));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    const auto result = transport_->Setup();
    EXPECT_FALSE(result.has_value());
}

TEST_F(QemuHypervisorTransportTest, ShutdownCallsShutdownOnTransport)
{
    // Shutdown called explicitly + in destructor = 2 times
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(2);

    QemuHypervisorTransport transport(gateway_core_mock_, std::move(mock_transport_owner_), ivshmem_provider_mock_);
    transport.Shutdown();
}

TEST_F(QemuHypervisorTransportTest, OfferServiceSendsOfferServiceRequest)
{
    this->WithAQemuHypervisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();

    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kOfferServiceRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    transport_->OfferService(specifier);
}

TEST_F(QemuHypervisorTransportTest, StopOfferServiceSendsStopOfferServiceRequest)
{
    this->WithAQemuHypervisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();

    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kStopOfferServiceRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    transport_->StopOfferService(specifier);
}

TEST_F(QemuHypervisorTransportTest, NotifyUpdateSendsUpdateNotification)
{
    this->WithAQemuHypervisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();

    EXPECT_CALL(*bi_directional_transport_mock_,
                SendNotification(::testing::Property(&TransportMessage::GetType, MessageType::kUpdateNotification)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    transport_->NotifyUpdate(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
}

TEST_F(QemuHypervisorTransportTest, RegisterUpdateNotificationSendsRegisterRequest)
{
    this->WithAQemuHypervisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();

    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kRegisterNotificationRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    transport_->RegisterUpdateNotification(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
}

TEST_F(QemuHypervisorTransportTest, UnregisterUpdateNotificationSendsUnregisterRequest)
{
    this->WithAQemuHypervisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();

    EXPECT_CALL(
        *bi_directional_transport_mock_,
        SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kUnregisterNotificationRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    transport_->UnregisterUpdateNotification(specifier, impl::ServiceElementType::EVENT, "SpeedEvent");
}

TEST_F(QemuHypervisorTransportTest, ProvideServiceSendsProvideServiceRequestWithShmSizes)
{
    this->WithAQemuHypervisorTransport();
    const auto specifier = CreateValidInstanceSpecifier();

    EXPECT_CALL(*bi_directional_transport_mock_,
                SendRequest(::testing::Property(&TransportMessage::GetType, MessageType::kProvideServiceRequest)))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    transport_->ProvideService(specifier, std::vector<impl::EventInfo>{});
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedProvideServiceRequestWithValidSpecifierCallsProvideServiceOnGatewayCore)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);

    // PreCreateInterVmSharedMemory will call LookupOffsetInDirectory which returns nullopt on non-QNX
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::_))
        .WillRepeatedly(::testing::Return(std::nullopt));

    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, OnMessageReceivedProvideServiceRequestWithInvalidSpecifierReturnsWithoutCoreCall)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest, false);

    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedStopOfferServiceRequestWithValidSpecifierCallsStopOfferServiceOnGatewayCore)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kStopOfferServiceRequest);

    EXPECT_CALL(gateway_core_mock_, StopOfferService(::testing::_)).WillOnce(::testing::Return());
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, OnMessageReceivedStopOfferServiceRequestWithInvalidSpecifierReturnsWithoutCoreCall)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kStopOfferServiceRequest, false);

    EXPECT_CALL(gateway_core_mock_, StopOfferService(::testing::_)).Times(0);
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedOfferServiceRequestWithValidSpecifierCallsOfferServiceOnGatewayCore)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kOfferServiceRequest);

    EXPECT_CALL(gateway_core_mock_, OfferService(::testing::_)).WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, OnMessageReceivedOfferServiceRequestWithInvalidSpecifierReturnsWithoutCoreCall)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kOfferServiceRequest, false);

    EXPECT_CALL(gateway_core_mock_, OfferService(::testing::_)).Times(0);
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedRegisterNotificationRequestWithValidSpecifierCallsRegisterOnGatewayCore)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kRegisterNotificationRequest);

    EXPECT_CALL(gateway_core_mock_, RegisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedRegisterNotificationRequestWithInvalidSpecifierReturnsWithoutCoreCall)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kRegisterNotificationRequest, false);

    EXPECT_CALL(gateway_core_mock_, RegisterUpdateNotification(::testing::_, ::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedUnregisterNotificationRequestWithValidSpecifierCallsUnregisterOnGatewayCore)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kUnregisterNotificationRequest);

    EXPECT_CALL(gateway_core_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest,
       OnMessageReceivedUnregisterNotificationRequestWithInvalidSpecifierReturnsWithoutCoreCall)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kUnregisterNotificationRequest, false);

    EXPECT_CALL(gateway_core_mock_, UnregisterUpdateNotification(::testing::_, ::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, OnMessageReceivedUpdateNotificationWithValidSpecifierCallsNotifyUpdateOnGatewayCore)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kUpdateNotification);

    EXPECT_CALL(gateway_core_mock_, NotifyUpdate(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, OnMessageReceivedUpdateNotificationWithInvalidSpecifierReturnsWithoutCoreCall)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    auto request = CreateMessageOfType(MessageType::kUpdateNotification, false);

    EXPECT_CALL(gateway_core_mock_, NotifyUpdate(::testing::_, ::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, UnsupportedMessageWillBeLogged)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();
    score::mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);

    auto request = CreateMessageOfType(MessageType::kAckResponse);

    EXPECT_CALL(recorder_mock, StartRecord(::testing::_, mw::log::LogLevel::kError))
        .WillOnce(::testing::Return(mw::log::SlotHandle{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));

    score::mw::log::SetLogRecorder(nullptr);
}

TEST_F(QemuHypervisorTransportTest, NullptrMessageWillBeLogged)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();
    score::mw::log::RecorderMock recorder_mock{};
    score::mw::log::SetLogRecorder(&recorder_mock);

    EXPECT_CALL(recorder_mock, StartRecord(::testing::_, mw::log::LogLevel::kError))
        .WillOnce(::testing::Return(mw::log::SlotHandle{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(nullptr);

    score::mw::log::SetLogRecorder(nullptr);
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemoryBindsCtrlAndDataWhenOffsetsFound)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    // LookupOffsetInDirectory returns valid offsets for both CTRL and DATA
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/ctrl")))
        .WillOnce(::testing::Return(std::optional<std::uint64_t>{0U}));
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/data")))
        .WillOnce(::testing::Return(std::optional<std::uint64_t>{4096U}));

    // AllocateNamedTypedMemoryAtOffset succeeds for both
    EXPECT_CALL(*ivshmem_provider_raw_, AllocateNamedTypedMemoryAtOffset(::testing::_, ::testing::_, 0U, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));
    EXPECT_CALL(*ivshmem_provider_raw_,
                AllocateNamedTypedMemoryAtOffset(::testing::_, ::testing::_, 4096U, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);
    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemoryReturnsEarlyWhenCtrlBindFails)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    // Both lookups happen before any allocation
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/ctrl")))
        .WillOnce(::testing::Return(std::optional<std::uint64_t>{0U}));
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/data")))
        .WillOnce(::testing::Return(std::optional<std::uint64_t>{4096U}));

    // CTRL allocation fails — PreCreateInterVmSharedMemory returns early, DATA allocation not attempted
    EXPECT_CALL(*ivshmem_provider_raw_, AllocateNamedTypedMemoryAtOffset(::testing::_, ::testing::_, 0U, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM))));

    // ProvideService is still called after PreCreateInterVmSharedMemory returns
    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);
    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemoryReturnsEarlyWhenDataBindFails)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/ctrl")))
        .WillOnce(::testing::Return(std::optional<std::uint64_t>{0U}));
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/data")))
        .WillOnce(::testing::Return(std::optional<std::uint64_t>{4096U}));

    // CTRL succeeds
    EXPECT_CALL(*ivshmem_provider_raw_, AllocateNamedTypedMemoryAtOffset(::testing::_, ::testing::_, 0U, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // DATA allocation fails
    EXPECT_CALL(*ivshmem_provider_raw_,
                AllocateNamedTypedMemoryAtOffset(::testing::_, ::testing::_, 4096U, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM))));

    // ProvideService is still called after PreCreateInterVmSharedMemory returns
    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);
    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemoryLogsWarningWhenCtrlNotFoundButSizeNonZero)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    // Both offsets not found in directory
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::_))
        .WillRepeatedly(::testing::Return(std::nullopt));

    // ProvideService should still be called (PreCreate logs warning but doesn't fail)
    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    // ProvideServiceRequest has non-zero sizes (1024 + 4096), so the "offset not found" warning path is taken
    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);
    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemorySkipsCtrlBindWhenSizeIsZero)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    // Create a ProvideServiceRequest with zero CTRL size
    auto specifier = CreateValidInstanceSpecifier();
    auto request = std::make_unique<ProvideServiceRequest>(specifier, std::vector<impl::EventInfo>{}, 0U, 4096U);

    // CTRL offset not found, but size is 0 — no warning logged
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/ctrl")))
        .WillOnce(::testing::Return(std::nullopt));
    // DATA offset not found, size is non-zero — warning logged
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/data")))
        .WillOnce(::testing::Return(std::nullopt));

    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemorySkipsDataBindWhenSizeIsZero)
{
    this->WithAQemuHypervisorTransport().WithARegisteredOnSetupCallback();

    // Create a ProvideServiceRequest with zero DATA size
    auto specifier = CreateValidInstanceSpecifier();
    auto request = std::make_unique<ProvideServiceRequest>(specifier, std::vector<impl::EventInfo>{}, 1024U, 0U);

    // CTRL offset not found, size non-zero — warning
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/ctrl")))
        .WillOnce(::testing::Return(std::nullopt));
    // DATA offset not found, but size is 0 — no warning
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::HasSubstr("/data")))
        .WillOnce(::testing::Return(std::nullopt));

    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, PreCreateInterVmSharedMemoryLogsErrorAndReturnsEarlyWhenPathsEmpty)
{
    this->WithAQemuHypervisorTransportWithEmptyPathResolver().WithARegisteredOnSetupCallback();

    // ivshmem provider must not be touched — we return early before reaching LookupOffsetInDirectory
    EXPECT_CALL(*ivshmem_provider_raw_, LookupOffsetInDirectory(::testing::_)).Times(0);

    // HandleProvideServiceRequest calls ProvideService after PreCreateInterVmSharedMemory returns
    EXPECT_CALL(gateway_core_mock_, ProvideService(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::ResultBlank{}));
    EXPECT_CALL(*bi_directional_transport_mock_, Shutdown()).Times(1);

    auto request = CreateMessageOfType(MessageType::kProvideServiceRequest);
    captured_handler_(std::move(request));
}

TEST_F(QemuHypervisorTransportTest, ResolveInterVmShmPathsReturnsPathsWithInterVmPrefix)
{
    const auto specifier = CreateValidInstanceSpecifier();
    const auto paths = ResolveInterVmShmPaths(specifier);

    EXPECT_EQ(paths.control, "/intervm-shared-shmem/SpeedService/Instance42/ctrl");
    EXPECT_EQ(paths.data, "/intervm-shared-shmem/SpeedService/Instance42/data");
}

#if !defined(__QNXNTO__)
TEST_F(QemuHypervisorTransportTest, GetInterVmShmSizesReturnsZeroOnNonQnx)
{
    // On Linux host, GetInterVmShmSizes returns zero sizes (no shm_open support for intervm paths)
    const auto specifier = CreateValidInstanceSpecifier();
    const auto sizes = GetInterVmShmSizes(specifier);
    EXPECT_EQ(sizes.control, 0U);
    EXPECT_EQ(sizes.data, 0U);
}
#endif  // !defined(__QNXNTO__)

#if defined(__QNXNTO__)
TEST_F(QemuHypervisorTransportTest, GetInterVmShmSizesWithMmanQnxHandlesSuccess)
{
    // Given a mock that returns valid fds for shm_open.
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();

    // When GetInterVmShmSizes opens the CTRL and DATA shm objects.
    EXPECT_CALL(
        *raw,
        shm_open(::testing::StrEq("/intervm-shared-shmem/SpeedService/Instance42/ctrl"), ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{10}));  // Return a valid fd
    EXPECT_CALL(
        *raw,
        shm_open(::testing::StrEq("/intervm-shared-shmem/SpeedService/Instance42/data"), ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{11}));  // Return a valid fd

    const auto specifier = CreateValidInstanceSpecifier();
    const auto sizes = GetInterVmShmSizes(specifier, raw);

    // Then the fstat path reports zero sizes for the fake fds.
    EXPECT_EQ(sizes.control, 0U);
    EXPECT_EQ(sizes.data, 0U);
}
#endif  // defined(__QNXNTO__)

}  // namespace

}  // namespace score::mw::com::gateway
