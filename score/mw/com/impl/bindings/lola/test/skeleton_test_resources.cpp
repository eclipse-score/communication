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
#include <functional>
#include <memory>

namespace score::mw::com::impl::lola
{

namespace
{

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::WithArg;

}  // namespace

LolaServiceInstanceDeployment CreateLolaServiceInstanceDeployment(
    std::uint16_t instance_id,
    std::vector<std::pair<std::string, LolaEventInstanceDeployment>> lola_event_inst_depls,
    std::vector<std::pair<std::string, LolaFieldInstanceDeployment>> lola_field_inst_depls,
    std::vector<std::pair<std::string, LolaMethodInstanceDeployment>> lola_method_inst_depls,
    std::vector<uid_t> allowed_consumers_qm,
    std::vector<uid_t> allowed_consumers_asil_b,
    score::cpp::optional<std::size_t> shm_size,
    score::cpp::optional<std::size_t> control_asil_b_shm_size,
    score::cpp::optional<std::size_t> control_qm_shm_size)
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
    lola_service_instance_deployment_.shared_memory_size_ = shm_size;
    lola_service_instance_deployment_.control_asil_b_memory_size_ = control_asil_b_shm_size;
    lola_service_instance_deployment_.control_qm_memory_size_ = control_qm_shm_size;

    for (auto lola_event_inst_depl : lola_event_inst_depls)
    {
        lola_service_instance_deployment_.events_.emplace(lola_event_inst_depl);
    }

    for (auto lola_field_inst_depl : lola_field_inst_depls)
    {
        lola_service_instance_deployment_.fields_.emplace(lola_field_inst_depl);
    }

    for (auto lola_method_inst_depl : lola_method_inst_depls)
    {
        lola_service_instance_deployment_.methods_.emplace(lola_method_inst_depl);
    }
    return lola_service_instance_deployment_;
}

ServiceTypeDeployment CreateTypeDeployment(const uint16_t lola_service_id,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& event_ids,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& field_ids,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& method_ids)
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

    LolaServiceTypeDeployment::MethodIdMapping method_id_mapping{};
    for (auto& method_with_id : method_ids)
    {
        method_id_mapping.insert(method_with_id);
    }

    return ServiceTypeDeployment{
        LolaServiceTypeDeployment(lola_service_id, event_id_mapping, field_id_mapping, method_id_mapping)};
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

InstanceIdentifier GetValidInstanceIdentifierWithMethods()
{
    return make_InstanceIdentifier(test::kValidInstanceDeploymentWithMethods, test::kValidTypeDeploymentWithMethods);
}

InstanceIdentifier GetValidASILInstanceIdentifierWithEvent()
{
    return make_InstanceIdentifier(test::kValidAsilInstanceDeploymentWithEvent, test::kValidTypeDeploymentWithEvent);
}

InstanceIdentifier GetValidASILInstanceIdentifierWithField()
{
    return make_InstanceIdentifier(test::kValidAsilInstanceDeploymentWithField, test::kValidTypeDeploymentWithField);
}

InstanceIdentifier GetValidASILInstanceIdentifierWithMethods()
{
    return make_InstanceIdentifier(test::kValidAsilInstanceDeploymentWithMethods,
                                   test::kValidTypeDeploymentWithMethods);
}

SkeletonMockedMemoryFixture::SkeletonMockedMemoryFixture()
{
    // Default behaviour for impl and lola runtimes
    impl::Runtime::InjectMock(&runtime_mock_);
    ON_CALL(runtime_mock_, GetBindingRuntime(BindingType::kLoLa)).WillByDefault(::testing::Return(&lola_runtime_mock_));
    memory::shared::SharedMemoryFactory::InjectMock(&shared_memory_factory_mock_);
    ON_CALL(*data_shared_memory_resource_mock_, IsShmInTypedMemory()).WillByDefault(::testing::Return(false));

    ON_CALL(runtime_mock_, GetTracingRuntime()).WillByDefault(Return(&tracing_runtime_mock_));
    ON_CALL(tracing_runtime_mock_, GetBindingTracingRuntime(BindingType::kLoLa))
        .WillByDefault(ReturnRef(binding_tracing_runtime_mock_));

    ON_CALL(lola_runtime_mock_, GetApplicationId()).WillByDefault(::testing::Return(kDummyApplicationId));
    ON_CALL(lola_runtime_mock_, GetLolaMessaging()).WillByDefault(::testing::ReturnRef(message_passing_mock_));

    ON_CALL(filesystem_fake_.GetUtils(), CreateDirectories(_, _)).WillByDefault(Return(score::ResultBlank{}));

    // Default behaviour for path builders
    ON_CALL(shm_path_builder_mock_, GetControlChannelShmName(_, QualityType::kASIL_QM))
        .WillByDefault(Return(test::kControlChannelPathQm));
    ON_CALL(shm_path_builder_mock_, GetControlChannelShmName(_, QualityType::kASIL_B))
        .WillByDefault(Return(test::kControlChannelPathAsilB));
    ON_CALL(shm_path_builder_mock_, GetDataChannelShmName(_)).WillByDefault(Return(test::kDataChannelPath));

    // Default behaviour for successful usage marker file creation
    ON_CALL(partial_restart_path_builder_mock_, GetServiceInstanceUsageMarkerFilePath(_))
        .WillByDefault(Return(test::kServiceInstanceUsageFilePath));
    ON_CALL(*fcntl_mock_, open(StrEq(test::kServiceInstanceUsageFilePath), test::kCreateOrOpenFlags, _))
        .WillByDefault(Return(test::kServiceInstanceUsageFileDescriptor));
    ON_CALL(*stat_mock_, chmod(StrEq(test::kServiceInstanceUsageFilePath), _)).WillByDefault(Return(score::cpp::blank{}));

    // Default behaviour for creating QM and ASIL-B shared memory resources - occurs when there is no connected proxy.
    ON_CALL(shared_memory_factory_mock_, Create(test::kControlChannelPathQm, _, _, _, false))
        .WillByDefault(
            WithArg<1>([this](auto initialize_callback) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                std::invoke(initialize_callback, control_qm_shared_memory_resource_mock_);
                return control_qm_shared_memory_resource_mock_;
            }));
    ON_CALL(shared_memory_factory_mock_, Create(test::kControlChannelPathAsilB, _, _, _, false))
        .WillByDefault(
            WithArg<1>([this](auto initialize_callback) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                std::invoke(initialize_callback, control_asil_b_shared_memory_resource_mock_);
                return control_asil_b_shared_memory_resource_mock_;
            }));

    // Default behaviour for opening QM and ASIL-B shared memory resources - occurs when there is a connected proxy.
    ON_CALL(shared_memory_factory_mock_, Open(test::kControlChannelPathQm, true, _))
        .WillByDefault(Return(control_qm_shared_memory_resource_mock_));
    ON_CALL(shared_memory_factory_mock_, Open(test::kControlChannelPathAsilB, true, _))
        .WillByDefault(Return(control_asil_b_shared_memory_resource_mock_));

    // Default behaviour for opening / creating data shared memory resource
    ON_CALL(shared_memory_factory_mock_, Create(test::kDataChannelPath, _, _, _, _))
        .WillByDefault(
            WithArg<1>([this](auto initialize_callback) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                std::invoke(initialize_callback, data_shared_memory_resource_mock_);
                return data_shared_memory_resource_mock_;
            }));
    ON_CALL(shared_memory_factory_mock_, Open(test::kDataChannelPath, true, _))
        .WillByDefault(Return(data_shared_memory_resource_mock_));

    // Construct ServiceDataControl / Storage using mocked memory resources
    service_data_control_qm_ = std::make_unique<ServiceDataControl>(
        CreateServiceDataControlWithEvent(test::kDummyElementFqId, QualityType::kASIL_QM));
    service_data_control_asil_b_ = std::make_unique<ServiceDataControl>(
        CreateServiceDataControlWithEvent(test::kDummyElementFqId, QualityType::kASIL_B));
    service_data_storage_ = std::make_unique<ServiceDataStorage>(
        CreateServiceDataStorageWithEvent<test::TestSampleType>(test::kDummyElementFqId));

    // Default behaviour for get the usable base addresses of the mocked memory resources using the constructed
    // ServiceDataControl / Storage created above.
    ON_CALL(*control_qm_shared_memory_resource_mock_, getUsableBaseAddress())
        .WillByDefault(Return(static_cast<void*>(service_data_control_qm_.get())));
    ON_CALL(*control_asil_b_shared_memory_resource_mock_, getUsableBaseAddress())
        .WillByDefault(Return(static_cast<void*>(service_data_control_asil_b_.get())));
    ON_CALL(*data_shared_memory_resource_mock_, getUsableBaseAddress())
        .WillByDefault(Return(static_cast<void*>(service_data_storage_.get())));
}

SkeletonMockedMemoryFixture::~SkeletonMockedMemoryFixture()
{
    score::memory::shared::MemoryResourceRegistry::getInstance().clear();
    impl::Runtime::InjectMock(nullptr);
    memory::shared::SharedMemoryFactory::InjectMock(nullptr);
}

SkeletonMockedMemoryFixture& SkeletonMockedMemoryFixture::InitialiseSkeleton(
    const InstanceIdentifier& instance_identifier)
{
    const auto& instance_depl_info = InstanceIdentifierView{instance_identifier}.GetServiceInstanceDeployment();
    const auto* lola_service_instance_deployment_ptr =
        std::get_if<LolaServiceInstanceDeployment>(&instance_depl_info.bindingInfo_);
    EXPECT_NE(lola_service_instance_deployment_ptr, nullptr);

    const auto& service_type_depl_info = InstanceIdentifierView{instance_identifier}.GetServiceTypeDeployment();
    const auto* lola_service_type_deployment_ptr =
        std::get_if<LolaServiceTypeDeployment>(&service_type_depl_info.binding_info_);
    EXPECT_NE(lola_service_type_deployment_ptr, nullptr);

    skeleton_ = std::make_unique<Skeleton>(
        instance_identifier,
        *lola_service_instance_deployment_ptr,
        *lola_service_type_deployment_ptr,
        filesystem_fake_.CreateInstance(),
        std::make_unique<ShmPathBuilderFacade>(shm_path_builder_mock_),
        std::make_unique<PartialRestartPathBuilderFacade>(partial_restart_path_builder_mock_),
        std::optional<memory::shared::LockFile>{},
        nullptr);

    return *this;
}

SkeletonMockedMemoryFixture& SkeletonMockedMemoryFixture::InitialiseSkeletonWithRealPathBuilders(
    const InstanceIdentifier& instance_identifier)
{
    const auto& instance_depl_info = InstanceIdentifierView{instance_identifier}.GetServiceInstanceDeployment();
    const auto* lola_service_instance_deployment_ptr =
        std::get_if<LolaServiceInstanceDeployment>(&instance_depl_info.bindingInfo_);
    EXPECT_NE(lola_service_instance_deployment_ptr, nullptr);

    const auto& service_type_depl_info = InstanceIdentifierView{instance_identifier}.GetServiceTypeDeployment();
    const auto* lola_service_type_deployment_ptr =
        std::get_if<LolaServiceTypeDeployment>(&service_type_depl_info.binding_info_);
    EXPECT_NE(lola_service_type_deployment_ptr, nullptr);

    skeleton_ = std::make_unique<Skeleton>(
        instance_identifier,
        *lola_service_instance_deployment_ptr,
        *lola_service_type_deployment_ptr,
        filesystem_fake_.CreateInstance(),
        std::make_unique<ShmPathBuilder>(lola_service_type_deployment_ptr->service_id_),
        std::make_unique<PartialRestartPathBuilder>(lola_service_type_deployment_ptr->service_id_),
        std::optional<memory::shared::LockFile>{},
        nullptr);

    return *this;
}

SkeletonMockedMemoryFixture& SkeletonMockedMemoryFixture::WithNoConnectedProxy()
{
    ON_CALL(*fcntl_mock_, flock(test::kServiceInstanceUsageFileDescriptor, test::kNonBlockingExlusiveLockOperation))
        .WillByDefault(Return(score::cpp::blank{}));
    ON_CALL(*fcntl_mock_, flock(test::kServiceInstanceUsageFileDescriptor, test::kUnlockOperation))
        .WillByDefault(Return(score::cpp::blank{}));
    return *this;
}

SkeletonMockedMemoryFixture& SkeletonMockedMemoryFixture::WithAlreadyConnectedProxy()
{
    ON_CALL(*fcntl_mock_, flock(test::kServiceInstanceUsageFileDescriptor, test::kNonBlockingExlusiveLockOperation))
        .WillByDefault(Return(score::cpp::make_unexpected(os::Error::createFromErrno(EWOULDBLOCK))));
    return *this;
}

void SkeletonMockedMemoryFixture::ExpectServiceUsageMarkerFileCreatedOrOpenedAndClosed() noexcept
{
    // Note: Default behaviour for these expectations are set in the constructor.
    EXPECT_CALL(partial_restart_path_builder_mock_, GetServiceInstanceUsageMarkerFilePath(_));
    EXPECT_CALL(*fcntl_mock_, open(StrEq(test::kServiceInstanceUsageFilePath), test::kCreateOrOpenFlags, _));
    EXPECT_CALL(*stat_mock_, chmod(StrEq(test::kServiceInstanceUsageFilePath), _));

    EXPECT_CALL(*unistd_mock_, close(test::kServiceInstanceUsageFileDescriptor));
    // we explicitly expect NO calls to unlink! See Skeleton::CreateOrOpenServiceInstanceUsageMarkerFile!
    EXPECT_CALL(*unistd_mock_, unlink(StrEq(test::kServiceInstanceUsageFilePath))).Times(0);
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
