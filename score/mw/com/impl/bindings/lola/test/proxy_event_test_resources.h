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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_PROXY_EVENT_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_PROXY_EVENT_TEST_RESOURCES_H

#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_mocked_service_data.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_mock.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime_binding.h"
#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/unistdmock.h"

#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/shared_memory_factory_mock.h"

#include <score/optional.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>

namespace score::mw::com::impl::lola
{

template <class EventSubscriptionControl>
class EventSubscriptionControlAttorney
{
  public:
    EventSubscriptionControlAttorney(EventSubscriptionControl& event_subscription_control) noexcept
        : event_subscription_control_{event_subscription_control}
    {
    }

    std::uint32_t GetCurrentState()
    {
        return event_subscription_control_.current_subscription_state_;
    }
    void SetCurrentState(std::uint32_t new_state)
    {
        event_subscription_control_.current_subscription_state_ = new_state;
    }

  private:
    EventSubscriptionControl& event_subscription_control_;
};

class GenericProxyEventAttorney
{
  public:
    using Callback = typename GenericProxyEvent::Callback;

    GenericProxyEventAttorney(GenericProxyEvent& generic_proxy_event) noexcept;

    auto& GetMetaInfoMember()
    {
        return generic_proxy_event_.meta_info_;
    }

  private:
    GenericProxyEvent& generic_proxy_event_;
};

template <typename T>
class ProxyEventAttorney
{
  public:
    using Callback = typename ProxyEvent<T>::Callback;

    ProxyEventAttorney(ProxyEvent<T>& proxy_event) noexcept : proxy_event_{proxy_event} {}

    auto& GetSamplesMember()
    {
        return proxy_event_.samples_;
    }

  private:
    ProxyEvent<T>& proxy_event_;
};

class ProxyEventCommonAttorney
{
  public:
    ProxyEventCommonAttorney(ProxyEventCommon& proxy_event_common) noexcept;

    void InjectSlotCollector(SlotCollector&& slot_collector);

  private:
    ProxyEventCommon& proxy_event_common_;
};

template <template <typename> class MessagePassingPtr>
class LolaRuntimeMock : public IRuntime
{
  public:
    explicit LolaRuntimeMock(bool has_asilb_support,
                             MessagePassingPtr<IMessagePassingService> message_passing_service = nullptr)
        : message_passing_service_{std::move(message_passing_service)}
    {
        EXPECT_CALL(*this, HasAsilBSupport).WillRepeatedly(::testing::Return(has_asilb_support));
        EXPECT_CALL(*this, GetBindingType).WillRepeatedly(::testing::Return(BindingType::kLoLa));
        if (message_passing_service_ != nullptr)
        {
            EXPECT_CALL(*this, GetLolaMessaging).WillRepeatedly(::testing::ReturnRef(*message_passing_service_));
        }
    }

    MOCK_METHOD(IMessagePassingService&, GetLolaMessaging, (), (noexcept, override));
    MOCK_METHOD(bool, HasAsilBSupport, (), (const, noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
    MOCK_METHOD(ShmSizeCalculationMode, GetShmSizeCalculationMode, (), (const, noexcept, override));
    MOCK_METHOD(impl::tracing::ITracingRuntimeBinding*, GetTracingRuntime, (), (noexcept, override));
    MOCK_METHOD(RollbackSynchronization&, GetRollbackSynchronization, (), (noexcept, override));
    MOCK_METHOD(pid_t, GetPid, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, GetApplicationId, (), (const, noexcept, override));

  private:
    MessagePassingPtr<IMessagePassingService> message_passing_service_{};
};

class RuntimeMockGuard
{
  public:
    RuntimeMockGuard()
    {
        impl::Runtime::InjectMock(&mock_);
    }
    ~RuntimeMockGuard()
    {
        impl::Runtime::InjectMock(nullptr);
    }

    impl::RuntimeMock mock_;
};

class SharedMemoryFactoryGuard
{
  public:
    SharedMemoryFactoryGuard()
    {
        memory::shared::SharedMemoryFactory::InjectMock(&mock_);
    }
    ~SharedMemoryFactoryGuard()
    {
        memory::shared::SharedMemoryFactory::InjectMock(nullptr);
    }

    memory::shared::SharedMemoryFactoryMock mock_;
};

class ProxyMockedMemoryFixture : public ::testing::Test
{
  protected:
    using SampleType = std::uint32_t;
    static constexpr pid_t kDummyPid{123456};

    ProxyMockedMemoryFixture() noexcept;

    void InitialiseProxyWithConstructor(const InstanceIdentifier& instance_identifier);
    void InitialiseProxyWithCreate(const InstanceIdentifier& instance_identifier);

    void ExpectControlSegmentOpened();
    void ExpectDataSegmentOpened();
    void InitialiseDummySkeletonEvent(const ElementFqId element_fq_id,
                                      const SkeletonEventProperties& skeleton_event_properties);

    LolaServiceInstanceId lola_service_instance_id_{0x10};
    LolaServiceId lola_service_id_{0xcdef};
    LolaServiceInstanceDeployment lola_service_instance_deployment_{lola_service_instance_id_};
    LolaServiceTypeDeployment lola_service_deployment_{lola_service_id_};
    ServiceIdentifierType service_identifier_ = make_ServiceIdentifierType("foo");
    ServiceTypeDeployment service_type_deployment_ = ServiceTypeDeployment{lola_service_deployment_};
    const InstanceSpecifier instance_specifier_{InstanceSpecifier::Create("/my_dummy_instance_specifier").value()};
    QualityType service_quality_type_{QualityType::kASIL_QM};
    ServiceInstanceDeployment service_instance_deployment_ =
        ServiceInstanceDeployment{service_identifier_,
                                  lola_service_instance_deployment_,
                                  service_quality_type_,
                                  instance_specifier_};
    InstanceIdentifier identifier_ = make_InstanceIdentifier(service_instance_deployment_, service_type_deployment_);

    RuntimeMockGuard runtime_mock_{};

    os::MockGuard<os::FcntlMock> fcntl_mock_{};
    os::MockGuard<os::UnistdMock> unistd_mock_{};
    SharedMemoryFactoryGuard shared_memory_factory_mock_guard_{};

    ServiceDiscoveryMock service_discovery_mock_{};

    FakeMockedServiceData fake_data_{kDummyPid};
    EventControl* event_control_{nullptr};
    EventDataStorage<SampleType>* event_data_storage_{nullptr};
    RollbackSynchronization rollback_synchronization_{};

    std::shared_ptr<MessagePassingServiceMock> mock_service_{std::make_shared<MessagePassingServiceMock>()};
    LolaRuntimeMock<std::shared_ptr> binding_runtime_{false, mock_service_};

    std::unique_ptr<Proxy> proxy_{nullptr};
};

class LolaProxyEventResources : public ProxyMockedMemoryFixture
{
  protected:
    LolaProxyEventResources();
    ~LolaProxyEventResources() override;

    std::future<std::shared_ptr<ScopedEventReceiveHandler>> ExpectRegisterEventNotification(
        score::cpp::optional<pid_t> pid = {});
    void ExpectReregisterEventNotification(score::cpp::optional<pid_t> pid = {});
    void ExpectUnregisterEventNotification(score::cpp::optional<pid_t> pid = {});

    SlotIndexType PutData(const std::uint32_t value = 42, const EventSlotStatus::EventTimeStamp timestamp = 1);

    const std::size_t max_num_slots_{5U};
    const std::uint8_t max_subscribers_{10U};

    const std::string event_name_{"dummy_event"};
    const std::uint8_t lola_element_id_{0x5};
    const ElementFqId element_fq_id_{lola_service_id_,
                                     lola_element_id_,
                                     lola_service_instance_id_.GetId(),
                                     ServiceElementType::EVENT};

    IMessagePassingService::HandlerRegistrationNoType current_subscription_no_ = 37U;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_PROXY_EVENT_TEST_RESOURCES_H
