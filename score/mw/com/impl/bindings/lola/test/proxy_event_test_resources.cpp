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
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/rollback_synchronization.h"

#include "score/memory/shared/memory_resource_registry.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace score::mw::com::impl::lola
{

namespace
{

const auto kControlChannelPrefix{"/lola-ctl-"};
const auto kDataChannelPrefix{"/lola-data-"};

using namespace ::score::memory::shared;

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StartsWith;

}  // namespace

GenericProxyEventAttorney::GenericProxyEventAttorney(GenericProxyEvent& generic_proxy_event) noexcept
    : generic_proxy_event_{generic_proxy_event}
{
}

ProxyEventCommonAttorney::ProxyEventCommonAttorney(ProxyEventCommon& proxy_event_common) noexcept
    : proxy_event_common_{proxy_event_common}
{
}

void ProxyEventCommonAttorney::InjectSlotCollector(SlotCollector&& slot_collector)
{
    proxy_event_common_.InjectSlotCollector(std::move(slot_collector));
}

ProxyMockedMemoryFixture::ProxyMockedMemoryFixture() noexcept
{
    ON_CALL(runtime_mock_.mock_, GetBindingRuntime(BindingType::kLoLa))
        .WillByDefault(::testing::Return(&binding_runtime_));
    ON_CALL(binding_runtime_, GetPid()).WillByDefault(::testing::Return(kDummyPid));
    ON_CALL(runtime_mock_.mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_mock_));

    ExpectControlSegmentOpened();
    ExpectDataSegmentOpened();

    ON_CALL(*(fake_data_.control_memory), getUsableBaseAddress())
        .WillByDefault(Return(static_cast<void*>(fake_data_.data_control)));
    ON_CALL(*(fake_data_.data_memory), getUsableBaseAddress())
        .WillByDefault(Return(static_cast<void*>(fake_data_.data_storage)));

    ON_CALL(binding_runtime_, GetRollbackSynchronization()).WillByDefault(ReturnRef(rollback_synchronization_));
}

void ProxyMockedMemoryFixture::ExpectControlSegmentOpened()
{
    ON_CALL(shared_memory_factory_mock_guard_.mock_, Open(StartsWith(kControlChannelPrefix), true, _))
        .WillByDefault(InvokeWithoutArgs([this]() -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
            return fake_data_.control_memory;
        }));
}

void ProxyMockedMemoryFixture::ExpectDataSegmentOpened()
{
    ON_CALL(shared_memory_factory_mock_guard_.mock_, Open(StartsWith(kDataChannelPrefix), false, _))
        .WillByDefault(InvokeWithoutArgs([this]() -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
            return fake_data_.data_memory;
        }));
}

void ProxyMockedMemoryFixture::InitialiseProxyWithConstructor(const InstanceIdentifier& instance_identifier)
{
    EnrichedInstanceIdentifier enriched_instance_identifier{instance_identifier};
    ON_CALL(service_discovery_mock_, StartFindService(_, enriched_instance_identifier))
        .WillByDefault(Return(make_FindServiceHandle(10U)));

    Proxy::EventNameToElementFqIdConverter event_name_to_element_fq_id_converter{lola_service_deployment_,
                                                                                 lola_service_instance_id_.GetId()};
    proxy_ = std::make_unique<Proxy>(fake_data_.control_memory,
                                     fake_data_.data_memory,
                                     service_quality_type_,
                                     std::move(event_name_to_element_fq_id_converter),
                                     make_HandleType(instance_identifier),
                                     std::optional<memory::shared::LockFile>{},
                                     nullptr);
}

void ProxyMockedMemoryFixture::InitialiseProxyWithCreate(const InstanceIdentifier& instance_identifier)
{
    EnrichedInstanceIdentifier enriched_instance_identifier{instance_identifier};
    ON_CALL(service_discovery_mock_, StartFindService(_, enriched_instance_identifier))
        .WillByDefault(Return(make_FindServiceHandle(10U)));

    proxy_ = Proxy::Create(make_HandleType(instance_identifier));
}

void ProxyMockedMemoryFixture::InitialiseDummySkeletonEvent(const ElementFqId element_fq_id,
                                                            const SkeletonEventProperties& skeleton_event_properties)
{
    std::tie(event_control_, event_data_storage_) =
        fake_data_.AddEvent<SampleType>(element_fq_id, skeleton_event_properties);
}

LolaProxyEventResources::LolaProxyEventResources() : ProxyMockedMemoryFixture{}
{
    InitialiseProxyWithConstructor(identifier_);
    InitialiseDummySkeletonEvent(element_fq_id_, SkeletonEventProperties{max_num_slots_, max_subscribers_, true});
}

LolaProxyEventResources::~LolaProxyEventResources()
{
    score::memory::shared::MemoryResourceRegistry::getInstance().clear();
}

std::future<std::shared_ptr<ScopedEventReceiveHandler>> LolaProxyEventResources::ExpectRegisterEventNotification(
    score::cpp::optional<pid_t> pid)
{
    const auto pid_to_use = pid.has_value() ? pid.value() : kDummyPid;

    // Since gtest currently seems to require that the provided lambdas are copyable, we can't move the
    // local_handler_promise into the state of the lambda using a generalized lambda captures. Instead, we pass it in
    // using a shared_ptr
    auto local_handler_promise = std::make_shared<std::promise<std::shared_ptr<ScopedEventReceiveHandler>>>();
    auto local_handler_future = local_handler_promise->get_future();

    EXPECT_CALL(*mock_service_,
                RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, ::testing::_, pid_to_use))
        .WillOnce(::testing::Invoke(
            ::testing::WithArg<2>([local_handler_promise](std::weak_ptr<ScopedEventReceiveHandler> handler_weak_ptr)
                                      -> IMessagePassingService::HandlerRegistrationNoType {
                auto handler_shared_ptr = handler_weak_ptr.lock();
                EXPECT_TRUE(handler_shared_ptr);
                local_handler_promise->set_value(std::move(handler_shared_ptr));
                IMessagePassingService::HandlerRegistrationNoType registration_no{0};
                return registration_no;
            })))
        .RetiresOnSaturation();
    return local_handler_future;
}

void LolaProxyEventResources::ExpectReregisterEventNotification(score::cpp::optional<pid_t> pid)
{
    const auto pid_to_use = pid.has_value() ? pid.value() : kDummyPid;
    EXPECT_CALL(*mock_service_, ReregisterEventNotification(QualityType::kASIL_QM, element_fq_id_, pid_to_use))
        .RetiresOnSaturation();
}

void LolaProxyEventResources::ExpectUnregisterEventNotification(score::cpp::optional<pid_t> pid)
{
    const auto pid_to_use = pid.has_value() ? pid.value() : kDummyPid;
    EXPECT_CALL(*mock_service_,
                UnregisterEventNotification(QualityType::kASIL_QM, element_fq_id_, ::testing::_, pid_to_use));
}

SlotIndexType LolaProxyEventResources::PutData(const std::uint32_t value,
                                               const EventSlotStatus::EventTimeStamp timestamp)
{
    auto slot_result = event_control_->data_control.AllocateNextSlot();
    EXPECT_TRUE(slot_result.IsValid());
    auto slot_index = slot_result.GetIndex();
    event_data_storage_->at(slot_index) = value;
    event_control_->data_control.EventReady(slot_result, timestamp);
    return slot_index;
}

}  // namespace score::mw::com::impl::lola
