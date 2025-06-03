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
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"

#include "score/mw/com/impl/configuration/quality_type.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/os/fcntl.h"
#include "score/result/result.h"

#include <gmock/gmock.h>

#include <unistd.h>
#include <stdint.h>

namespace score::mw::com::impl::lola
{

namespace
{

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;

const score::os::Fcntl::Open kCreateOrOpenFlags{score::os::Fcntl::Open::kCreate | score::os::Fcntl::Open::kReadOnly};

}  // namespace

LolaServiceInstanceDeployment CreateLolaServiceInstanceDeployment(
    std::uint16_t instance_id,
    std::vector<std::pair<std::string, LolaEventInstanceDeployment>> lola_event_inst_depls,
    std::vector<std::pair<std::string, LolaFieldInstanceDeployment>> lola_field_inst_depls,
    std::vector<uid_t> allowed_consumers_qm,
    std::vector<uid_t> allowed_consumers_asil_b,
    score::cpp::optional<std::size_t> size)
{
    LolaServiceInstanceDeployment lola_service_instance_deployment_{LolaServiceInstanceId{instance_id}};
    for (auto user_id : allowed_consumers_qm)
    {
        lola_service_instance_deployment_.allowed_consumer_[QualityType::kASIL_QM].push_back(user_id);
    }
    for (auto user_id : allowed_consumers_asil_b)
    {
        lola_service_instance_deployment_.allowed_consumer_[QualityType::kASIL_B].push_back(user_id);
    }
    lola_service_instance_deployment_.shared_memory_size_ = size;

    for (auto lola_event_inst_depl : lola_event_inst_depls)
    {
        lola_service_instance_deployment_.events_.emplace(lola_event_inst_depl);
    }

    for (auto lola_field_inst_depl : lola_field_inst_depls)
    {
        lola_service_instance_deployment_.fields_.emplace(lola_field_inst_depl);
    }
    return lola_service_instance_deployment_;
}

ServiceTypeDeployment CreateTypeDeployment(const uint16_t lola_service_id,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& event_ids,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& field_ids)
{
    LolaServiceTypeDeployment::EventIdMapping event_id_mapping{};
    for (auto& event_with_id : event_ids)
    {
        event_id_mapping.insert(event_with_id);
    }

    LolaServiceTypeDeployment::FieldIdMapping field_id_mapping{};
    for (auto& field_with_id : field_ids)
    {
        field_id_mapping.insert(field_with_id);
    }

    return ServiceTypeDeployment{LolaServiceTypeDeployment(lola_service_id, event_id_mapping, field_id_mapping)};
}

bool fileExists(const std::string& filePath)
{
    return access(filePath.c_str(), F_OK) == 0;
}

InstanceIdentifier GetValidInstanceIdentifier()
{
    return make_InstanceIdentifier(test::kValidMinimalQmInstanceDeployment, test::kValidMinimalTypeDeployment);
}

InstanceIdentifier GetValidASILInstanceIdentifier()
{
    return make_InstanceIdentifier(test::kValidMinimalAsilInstanceDeployment, test::kValidMinimalTypeDeployment);
}

InstanceIdentifier GetValidASILInstanceIdentifierWithACL()
{
    return make_InstanceIdentifier(test::kValidMinimalAsilInstanceDeploymentWithAcl, test::kValidMinimalTypeDeployment);
}

InstanceIdentifier GetValidInstanceIdentifierWithEvent()
{
    return make_InstanceIdentifier(test::kValidInstanceDeploymentWithEvent, test::kValidTypeDeploymentWithEvent);
}

InstanceIdentifier GetValidInstanceIdentifierWithField()
{
    return make_InstanceIdentifier(test::kValidInstanceDeploymentWithField, test::kValidTypeDeploymentWithField);
}

InstanceIdentifier GetValidASILInstanceIdentifierWithEvent()
{
    return make_InstanceIdentifier(test::kValidAsilInstanceDeploymentWithEvent, test::kValidTypeDeploymentWithEvent);
}

InstanceIdentifier GetValidASILInstanceIdentifierWithField()
{
    return make_InstanceIdentifier(test::kValidAsilInstanceDeploymentWithField, test::kValidTypeDeploymentWithField);
}

SkeletonMockedMemoryFixture::SkeletonMockedMemoryFixture()
{
    impl::Runtime::InjectMock(&runtime_mock_);
    ON_CALL(runtime_mock_, GetBindingRuntime(BindingType::kLoLa)).WillByDefault(::testing::Return(&lola_runtime_mock_));
    memory::shared::SharedMemoryFactory::InjectMock(&shared_memory_factory_mock_);
    ON_CALL(*data_shared_memory_resource_mock_, IsShmInTypedMemory()).WillByDefault(::testing::Return(false));

    ON_CALL(runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    ON_CALL(tracing_runtime_mock_, GetTracingRuntimeBinding(BindingType::kLoLa))
        .WillByDefault(ReturnRef(tracing_runtime_binding_mock_));
}

SkeletonMockedMemoryFixture::~SkeletonMockedMemoryFixture()
{
    score::memory::shared::MemoryResourceRegistry::getInstance().clear();
    impl::Runtime::InjectMock(nullptr);
    memory::shared::SharedMemoryFactory::InjectMock(nullptr);
}

void SkeletonMockedMemoryFixture::InitialiseSkeleton(const InstanceIdentifier& instance_identifier)
{
    const auto& instance_depl_info = InstanceIdentifierView{instance_identifier}.GetServiceInstanceDeployment();
    const auto* lola_service_instance_deployment_ptr =
        std::get_if<LolaServiceInstanceDeployment>(&instance_depl_info.bindingInfo_);
    ASSERT_NE(lola_service_instance_deployment_ptr, nullptr);

    const auto& service_type_depl_info = InstanceIdentifierView{instance_identifier}.GetServiceTypeDeployment();
    const auto* lola_service_type_deployment_ptr =
        std::get_if<LolaServiceTypeDeployment>(&service_type_depl_info.binding_info_);
    ASSERT_NE(lola_service_type_deployment_ptr, nullptr);

    skeleton_ = std::make_unique<Skeleton>(instance_identifier,
                                           *lola_service_instance_deployment_ptr,
                                           *lola_service_type_deployment_ptr,
                                           filesystem_fake_.CreateInstance(),
                                           std::make_unique<ShmPathBuilderMock>(),
                                           std::make_unique<PartialRestartPathBuilderMock>(),
                                           std::optional<memory::shared::LockFile>{},
                                           nullptr);

    SkeletonAttorney skeleton_attorney{*skeleton_};
    shm_path_builder_mock_ = skeleton_attorney.GetIShmPathBuilder();
    ASSERT_NE(shm_path_builder_mock_, nullptr);
    partial_restart_path_builder_mock_ = skeleton_attorney.GetIPartialRestartPathBuilder();
    ASSERT_NE(partial_restart_path_builder_mock_, nullptr);
    ON_CALL(filesystem_fake_.GetUtils(), CreateDirectories(_, _)).WillByDefault(Return(score::ResultBlank{}));

    ON_CALL(*shm_path_builder_mock_, GetControlChannelShmName(_, QualityType::kASIL_QM))
        .WillByDefault(Return(test::kControlChannelPathQm));
    ON_CALL(*shm_path_builder_mock_, GetControlChannelShmName(_, QualityType::kASIL_B))
        .WillByDefault(Return(test::kControlChannelPathAsilB));
    ON_CALL(*shm_path_builder_mock_, GetDataChannelShmName(_)).WillByDefault(Return(test::kDataChannelPath));
}

void SkeletonMockedMemoryFixture::ExpectServiceUsageMarkerFileCreatedOrOpenedAndClosed(
    const std::string& service_existence_marker_file_path,
    const std::int32_t lock_file_descriptor) noexcept
{
    EXPECT_CALL(*partial_restart_path_builder_mock_, GetServiceInstanceUsageMarkerFilePath(_))
        .WillOnce(Return(service_existence_marker_file_path));
    EXPECT_CALL(*fcntl_mock_, open(StrEq(service_existence_marker_file_path.data()), kCreateOrOpenFlags, _))
        .WillOnce(Return(lock_file_descriptor));
    EXPECT_CALL(*stat_mock_, chmod(StrEq(service_existence_marker_file_path.data()), _)).WillOnce(Return(score::cpp::blank{}));

    EXPECT_CALL(*unistd_mock_, close(lock_file_descriptor));
    // we explicitly expect NO calls to unlink! See Skeleton::CreateOrOpenServiceInstanceUsageMarkerFile!
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(service_existence_marker_file_path.data()))).Times(0);
}

void SkeletonMockedMemoryFixture::ExpectServiceUsageMarkerFileFlockAcquired(
    const std::int32_t existence_marker_file_descriptor) noexcept
{
    const os::Fcntl::Operation non_blocking_exclusive_lock_operation =
        os::Fcntl::Operation::kLockExclusive | score::os::Fcntl::Operation::kLockNB;
    const os::Fcntl::Operation unlock_operation = os::Fcntl::Operation::kUnLock;

    EXPECT_CALL(*fcntl_mock_, flock(existence_marker_file_descriptor, non_blocking_exclusive_lock_operation))
        .WillOnce(Return(score::cpp::blank{}));
    EXPECT_CALL(*fcntl_mock_, flock(existence_marker_file_descriptor, unlock_operation)).WillOnce(Return(score::cpp::blank{}));
}

void SkeletonMockedMemoryFixture::ExpectServiceUsageMarkerFileAlreadyFlocked(
    const std::int32_t existence_marker_file_descriptor) noexcept
{
    const os::Fcntl::Operation non_blocking_exclusive_lock_operation =
        os::Fcntl::Operation::kLockExclusive | score::os::Fcntl::Operation::kLockNB;

    EXPECT_CALL(*fcntl_mock_, flock(existence_marker_file_descriptor, non_blocking_exclusive_lock_operation))
        .WillOnce(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EWOULDBLOCK))));
}

void SkeletonMockedMemoryFixture::ExpectControlSegmentCreated(const QualityType quality_type)
{
    const auto control_channel_path =
        (quality_type == QualityType::kASIL_QM) ? test::kControlChannelPathQm : test::kControlChannelPathAsilB;
    const auto created_resource = (quality_type == QualityType::kASIL_QM) ? control_qm_shared_memory_resource_mock_
                                                                          : control_asil_b_shared_memory_resource_mock_;

    // When we attempt to create a shared memory region which returns a pointer to a resource
    EXPECT_CALL(shared_memory_factory_mock_, Create(control_channel_path, _, _, WritablePermissionsMatcher(), false))
        .WillOnce(InvokeWithoutArgs(
            [created_resource, quality_type, this]() -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                SkeletonAttorney skeleton_attorney{*skeleton_};
                skeleton_attorney.InitializeSharedMemoryForControl(quality_type, created_resource);
                return created_resource;
            }));
}

void SkeletonMockedMemoryFixture::ExpectDataSegmentCreated(const bool in_typed_memory)
{
    // When we attempt to create a shared memory region which returns a pointer to a resource
    EXPECT_CALL(shared_memory_factory_mock_,
                Create(test::kDataChannelPath, _, _, ReadablePermissionsMatcher(), in_typed_memory))
        .WillOnce(InvokeWithoutArgs([this]() -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
            SkeletonAttorney skeleton_attorney{*skeleton_};
            skeleton_attorney.InitializeSharedMemoryForData(data_shared_memory_resource_mock_);
            return data_shared_memory_resource_mock_;
        }));
}

void SkeletonMockedMemoryFixture::ExpectControlSegmentOpened(const QualityType quality_type,
                                                             ServiceDataControl& existing_service_data_control) noexcept
{
    const auto control_channel_path =
        (quality_type == QualityType::kASIL_QM) ? test::kControlChannelPathQm : test::kControlChannelPathAsilB;
    const auto created_resource = (quality_type == QualityType::kASIL_QM) ? control_qm_shared_memory_resource_mock_
                                                                          : control_asil_b_shared_memory_resource_mock_;

    // When we attempt to open a shared memory region which returns a pointer to a resource
    EXPECT_CALL(shared_memory_factory_mock_, Open(control_channel_path, true, _)).WillOnce(Return(created_resource));

    EXPECT_CALL(*created_resource, getUsableBaseAddress())
        .WillOnce(Return(static_cast<void*>(&existing_service_data_control)));
}

void SkeletonMockedMemoryFixture::ExpectDataSegmentOpened(ServiceDataStorage& existing_service_data_storage) noexcept
{
    // When we attempt to open a shared memory region which returns a pointer to a resource
    EXPECT_CALL(shared_memory_factory_mock_, Open(test::kDataChannelPath, true, _))
        .WillOnce(Return(data_shared_memory_resource_mock_));

    EXPECT_CALL(*data_shared_memory_resource_mock_, getUsableBaseAddress())
        .WillOnce(Return(static_cast<void*>(&existing_service_data_storage)));
}

ServiceDataControl SkeletonMockedMemoryFixture::CreateServiceDataControlWithEvent(ElementFqId element_fq_id,
                                                                                  QualityType quality_type) noexcept
{
    const auto created_resource = (quality_type == QualityType::kASIL_QM) ? control_qm_shared_memory_resource_mock_
                                                                          : control_asil_b_shared_memory_resource_mock_;
    ServiceDataControl service_data_control{created_resource->getMemoryResourceProxy()};

    auto event_control = service_data_control.event_controls_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(element_fq_id),
        std::forward_as_tuple(10U, 10U, true, created_resource->getMemoryResourceProxy()));
    EXPECT_TRUE(event_control.second);
    return service_data_control;
}

EventControl& SkeletonMockedMemoryFixture::GetEventControlFromServiceDataControl(
    ElementFqId element_fq_id,
    ServiceDataControl& service_data_control) noexcept
{
    auto event_control_it = service_data_control.event_controls_.find(element_fq_id);
    EXPECT_NE(event_control_it, service_data_control.event_controls_.cend());
    auto& event_control = event_control_it->second;
    return event_control;
}

void SkeletonMockedMemoryFixture::CleanUpSkeleton()
{
    // This function needs to be called, when the instance identifier for a class is located on the stack of the test
    // Because the skeleton will hold a raw pointer to that configuration item and on destruction of the SkeletonGuard
    // as member of this fixture, will invoke "StopOffer" which needs to access this configuration items.
    // Thus, we need to clean up earlier - which will cause now mock calls which have not been there before
    EXPECT_CALL(shared_memory_factory_mock_, Remove(_)).WillRepeatedly([](auto) {});
    skeleton_.reset();
}

}  // namespace score::mw::com::impl::lola
