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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_TEST_RESOURCES_H

#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/partial_restart_path_builder.h"
#include "score/mw/com/impl/bindings/lola/partial_restart_path_builder_mock.h"
#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder_mock.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/tracing/tracing_runtime.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"

#include "score/filesystem/factory/filesystem_factory_fake.h"
#include "score/filesystem/filesystem.h"
#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/shared_memory_factory_mock.h"
#include "score/memory/shared/shared_memory_resource_heap_allocator_mock.h"
#include "score/memory/shared/shared_memory_resource_mock.h"
#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/stat_mock.h"
#include "score/os/mocklib/unistdmock.h"

#include <score/optional.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{

LolaServiceInstanceDeployment CreateLolaServiceInstanceDeployment(
    LolaServiceInstanceId::InstanceId instance_id,
    std::vector<std::pair<std::string, LolaEventInstanceDeployment>> lola_event_inst_depls,
    std::vector<std::pair<std::string, LolaFieldInstanceDeployment>> lola_field_inst_depls,
    std::vector<std::pair<std::string, LolaMethodInstanceDeployment>> lola_method_inst_depls,
    std::vector<uid_t> allowed_consumers_qm,
    std::vector<uid_t> allowed_consumers_asil_b,
    score::cpp::optional<std::size_t> size = score::cpp::nullopt,
    score::cpp::optional<std::size_t> control_asil_b_shm_size = score::cpp::nullopt,
    score::cpp::optional<std::size_t> control_qm_shm_size = score::cpp::nullopt);

/// \brief Creates a ServiceTypeDeployment, which is effectively a LolaServiceTypeDeployment as we currently do not
///        support any other.
/// \param lola_service_id
/// \param event_ids vector of pairs of event-short-name and Lola specific id for the event.
/// \param field_ids vector of pairs of field-short-name and Lola specific id for the field.
/// \param method_ids vector of pairs of method-short-name and Lola specific id for the method.
/// \return
ServiceTypeDeployment CreateTypeDeployment(uint16_t lola_service_id,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& event_ids,
                                           const std::vector<std::pair<std::string, std::uint8_t>>& field_ids = {},
                                           const std::vector<std::pair<std::string, std::uint8_t>>& method_ids = {});

/// \brief Construct and returns a valid instance identifier from which a (Lola) skeleton instance (QM) can be
///        created.
/// \return an InstanceIdentifier created from our very basic/minimal type and instance deployments held by the
///         fixture.
InstanceIdentifier GetValidInstanceIdentifier();

/// \brief Construct and returns a valid instance identifier from which a (Lola) skeleton instance (QM ad ASIL-B)
///        can be created.
/// \return an InstanceIdentifier created from our very basic/minimal type and instance deployments held by the
///         fixture.
InstanceIdentifier GetValidASILInstanceIdentifier();

/// \brief Construct and returns a valid instance identifier from which a (Lola) skeleton instance (QM ad ASIL-B)
///        can be created.
/// \return an InstanceIdentifier created from our very basic/minimal type and instance deployments with ACL, which
///         has two user-ids for allowed QM-consumers (uid 42) and ASIL-B-consumers (uid 43) held by the fixture.
InstanceIdentifier GetValidASILInstanceIdentifierWithACL();

InstanceIdentifier GetValidInstanceIdentifierWithEvent();

InstanceIdentifier GetValidInstanceIdentifierWithField();

InstanceIdentifier GetValidInstanceIdentifierWithMethods();

InstanceIdentifier GetValidASILInstanceIdentifierWithEvent();

InstanceIdentifier GetValidASILInstanceIdentifierWithField();

InstanceIdentifier GetValidASILInstanceIdentifierWithMethods();

/// @brief Function to check whether a file exists which works on linux and QNX
bool fileExists(const std::string& filePath);

MATCHER(WritablePermissionsMatcher, "Permission is not writeable!")
{
    return std::holds_alternative<memory::shared::SharedMemoryFactory::WorldWritable>(arg);
}

MATCHER(ReadablePermissionsMatcher, "Permission is not readable!")
{
    return std::holds_alternative<memory::shared::SharedMemoryFactory::WorldReadable>(arg);
}

namespace test
{

using TestSampleType = std::uint8_t;

constexpr std::uint64_t kControlQmMemoryResourceId{0x01234567};
constexpr std::uint64_t kControlAsilBMemoryResourceId{0x12345678};
constexpr std::uint64_t kDataMemoryResourceId{0x23456789};
constexpr std::size_t kMaxSlots{10U};

/// for our very basic valid deployment, we use a configured shm-size of 500
static constexpr std::size_t kConfiguredDeploymentShmSize{1024U};
static constexpr std::size_t kConfiguredDeploymentControlAsilBShmSize{1024U};
static constexpr std::size_t kConfiguredDeploymentControlQmShmSize{1024U};
static constexpr LolaServiceInstanceId::InstanceId kDefaultLolaInstanceId{16U};

static const auto kFooEventName{"fooEvent"};
static const auto kDumbEventName{"dumbEvent"};
static const auto kFooFieldName{"fooField"};
static const auto kFooMethodName{"fooMethod"};
static const auto kDumbMethodName{"barMethod"};

static const SkeletonEventProperties kDefaultEventProperties{10, 5, true};

static constexpr std::uint16_t kFooEventId{1U};
static constexpr std::uint16_t kDumbEventId{2U};
static constexpr std::uint16_t kFooFieldId{3U};
static constexpr std::uint16_t kFooMethodId{4U};
static constexpr std::uint16_t kDumbMethodId{5U};

static constexpr LolaMethodInstanceDeployment::QueueSize kFooMethodQueueSize{5U};
static constexpr LolaMethodInstanceDeployment::QueueSize kDumbMethodQueueSize{6U};

static constexpr uid_t kAllowedQmMethodConsumer{20U};
static constexpr uid_t kAllowedAsilBMethodConsumer{21U};

static const auto kServiceTypeName{"foo"};
static const ServiceIdentifierType kFooService{make_ServiceIdentifierType(kServiceTypeName)};

const auto kFooInstanceSpecifier = InstanceSpecifier::Create(std::string{"foo/abc/TirePressurePort"}).value();

/// \brief A very basic (Lola) ASIL-QM only ServiceInstanceDeployment, which relates to the kValidMinimalTypeDeployment
///        and has a shm-size configuration of kConfiguredDeploymentShmSize. It does NOT use ACLs and therefore its
///        crl-section is "world-readable".
/// \note since this instance-deployment (and the type-deployment it relates to) has NO events, the decision to have
///       a configured shm-size was as follows: If no shm-size (its an optional deployment setting) is given, the
///       needed shm-size will be calculated based on the structure/content of the service. The main influence here
///       is the number/type/max-samples of the contained events! But since we have NO events in this basic setup,
///       the shm-size calculation is very "uninteresting". Ergo -> decision to chose the setting of shm-size in the
///       deployment.
static const ServiceInstanceDeployment kValidMinimalQmInstanceDeployment{
    kFooService,
    CreateLolaServiceInstanceDeployment(kDefaultLolaInstanceId,
                                        {},
                                        {},
                                        {},
                                        {},
                                        {},
                                        kConfiguredDeploymentShmSize,
                                        kConfiguredDeploymentControlAsilBShmSize,
                                        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_QM,
    kFooInstanceSpecifier};

/// \brief A very basic (Lola) ASIL-QM and ASIL-B ServiceInstanceDeployment, which relates to the
///        kValidMinimalTypeDeployment and has a shm-size configuration of 500.
/// \note same setup as valid_qm_instance_deployment, but ASIL-QM and ASIL-B.
static const ServiceInstanceDeployment kValidMinimalAsilInstanceDeployment{
    kFooService,
    CreateLolaServiceInstanceDeployment(kDefaultLolaInstanceId,
                                        {},
                                        {},
                                        {},
                                        {},
                                        {},
                                        kConfiguredDeploymentShmSize,
                                        kConfiguredDeploymentControlAsilBShmSize,
                                        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_B,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidMinimalAsilInstanceDeploymentWithAcl{
    kFooService,
    CreateLolaServiceInstanceDeployment(kDefaultLolaInstanceId,
                                        {},
                                        {},
                                        {},
                                        {42U},
                                        {43},
                                        kConfiguredDeploymentShmSize,
                                        kConfiguredDeploymentControlAsilBShmSize,
                                        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_B,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidInstanceDeploymentWithEvent{
    kFooService,
    CreateLolaServiceInstanceDeployment(
        kDefaultLolaInstanceId,
        {{test::kFooEventName, LolaEventInstanceDeployment{test::kMaxSlots, 10U, 1U, true, 0}}},
        {},
        {},
        {},
        {},
        kConfiguredDeploymentShmSize,
        kConfiguredDeploymentControlAsilBShmSize,
        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_QM,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidInstanceDeploymentWithField{
    kFooService,
    CreateLolaServiceInstanceDeployment(
        kDefaultLolaInstanceId,
        {},
        {{test::kFooEventName, LolaFieldInstanceDeployment{test::kMaxSlots, 10U, 1U, true, 0}}},
        {},
        {},
        {},
        kConfiguredDeploymentShmSize,
        kConfiguredDeploymentControlAsilBShmSize,
        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_QM,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidInstanceDeploymentWithMethods{
    kFooService,
    CreateLolaServiceInstanceDeployment(kDefaultLolaInstanceId,
                                        {},
                                        {},
                                        {{test::kFooMethodName, LolaMethodInstanceDeployment{kFooMethodQueueSize}},
                                         {test::kDumbMethodName, LolaMethodInstanceDeployment{kDumbMethodQueueSize}}},
                                        {kAllowedQmMethodConsumer},
                                        {},
                                        kConfiguredDeploymentShmSize,
                                        kConfiguredDeploymentControlAsilBShmSize,
                                        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_QM,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidAsilInstanceDeploymentWithEvent{
    kFooService,
    CreateLolaServiceInstanceDeployment(
        kDefaultLolaInstanceId,
        {{test::kFooEventName, LolaEventInstanceDeployment{test::kMaxSlots, 10U, 1U, true, 0}}},
        {},
        {},
        {},
        {},
        kConfiguredDeploymentShmSize,
        kConfiguredDeploymentControlAsilBShmSize,
        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_B,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidAsilInstanceDeploymentWithField{
    kFooService,
    CreateLolaServiceInstanceDeployment(
        kDefaultLolaInstanceId,
        {},
        {{test::kFooEventName, LolaFieldInstanceDeployment{test::kMaxSlots, 10U, 1U, true, 0}}},
        {},
        {},
        {},
        kConfiguredDeploymentShmSize,
        kConfiguredDeploymentControlAsilBShmSize,
        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_B,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidAsilInstanceDeploymentWithMethods{
    kFooService,
    CreateLolaServiceInstanceDeployment(kDefaultLolaInstanceId,
                                        {},
                                        {},
                                        {{test::kFooMethodName, LolaMethodInstanceDeployment{kFooMethodQueueSize}},
                                         {test::kDumbMethodName, LolaMethodInstanceDeployment{kDumbMethodQueueSize}}},
                                        {},
                                        {kAllowedAsilBMethodConsumer},
                                        kConfiguredDeploymentShmSize,
                                        kConfiguredDeploymentControlAsilBShmSize,
                                        kConfiguredDeploymentControlQmShmSize),
    QualityType::kASIL_B,
    kFooInstanceSpecifier};

static const ServiceInstanceDeployment kValidMinimalQmInstanceDeploymentWithBlankBinding{kFooService,
                                                                                         score::cpp::blank{},
                                                                                         QualityType::kASIL_QM,
                                                                                         kFooInstanceSpecifier};

static const uint16_t kLolaServiceId{1U};

/// \brief A very basic (Lola) ServiceTypeDeployment, which just contains a service-id and NO events at all!
/// \details For some of the basic tests, this is sufficient and since services without events are a valid use case
///          (at least later, when we also support fields/service-methods).
static const ServiceTypeDeployment kValidMinimalTypeDeployment{CreateTypeDeployment(kLolaServiceId, {})};

static const ServiceTypeDeployment kValidTypeDeploymentWithEvent{
    CreateTypeDeployment(kLolaServiceId, {{kFooEventName, kFooEventId}})};

static const ServiceTypeDeployment kValidTypeDeploymentWithField{
    CreateTypeDeployment(kLolaServiceId, {{kFooEventName, kFooEventId}})};

static const ServiceTypeDeployment kValidTypeDeploymentWithMethods{
    CreateTypeDeployment(kLolaServiceId, {}, {}, {{kFooMethodName, kFooMethodId}, {kDumbMethodName, kDumbMethodId}})};

static const ServiceTypeDeployment kValidMinimalTypeDeploymentWithBlankBinding{score::cpp::blank{}};

static const score::cpp::optional<std::size_t> kSimulatedShmSize{};

const auto kControlChannelPathQm{"/lola-ctl-0000000000000001-00016"};
const auto kControlChannelPathAsilB{"/lola-ctl-0000000000000001-00016-b"};
const auto kDataChannelPath{"/lola-data-0000000000000001-00016"};

static const std::string kServiceInstanceUsageFilePath{"/test_service_instance_usage_file_path"};
static const std::int32_t kServiceInstanceUsageFileDescriptor{7890};

static const score::os::Fcntl::Open kCreateOrOpenFlags{score::os::Fcntl::Open::kCreate | score::os::Fcntl::Open::kReadOnly};

static const os::Fcntl::Operation kNonBlockingExlusiveLockOperation =
    os::Fcntl::Operation::kLockExclusive | score::os::Fcntl::Operation::kLockNB;
static const os::Fcntl::Operation kUnlockOperation = os::Fcntl::Operation::kUnLock;

static const ElementFqId kDummyElementFqId{1U, 2U, 3U, ServiceElementType::EVENT};

}  // namespace test

class SkeletonAttorney
{
  public:
    SkeletonAttorney(Skeleton& skeleton) noexcept : skeleton_{skeleton} {}

    ServiceDataControl* GetServiceDataControl(const QualityType quality_type) const noexcept
    {
        if (quality_type == QualityType::kASIL_QM)
        {
            return skeleton_.control_qm_;
        }
        if (quality_type == QualityType::kASIL_B)
        {
            return skeleton_.control_asil_b_;
        }
        return nullptr;
    }

  private:
    Skeleton& skeleton_;
};

class SkeletonMockedMemoryFixture : public ::testing::Test
{
  public:
    static constexpr GlobalConfiguration::ApplicationId kDummyApplicationId{6543};

    // Use constructor / destructor instead of SetUp() / TearDown() so that they will always be called when
    // instantiating fixtures deriving from this class. Using SetUp() / TearDown() requires that child classes manually
    // call this classes SetUp() / TearDown() methods if they implement their own SetUp() / TearDown().
    SkeletonMockedMemoryFixture();
    virtual ~SkeletonMockedMemoryFixture();

    SkeletonMockedMemoryFixture& InitialiseSkeleton(const InstanceIdentifier& instance_identifier);

    SkeletonMockedMemoryFixture& InitialiseSkeletonWithRealPathBuilders(const InstanceIdentifier& instance_identifier);

    /// \brief Simulates that the instance usage marker file could be exclusively flocked meaning that no Procies are
    /// using an old shared memory region from this service. This is the "normal" case when we aren't in a partial
    /// restart scenario.
    SkeletonMockedMemoryFixture& WithNoConnectedProxy();

    /// \brief Simulates that the instance usage marker file could not be exclusively flocked meaning that a Proxy is
    /// still using an old shared memory region from this service. This occurs when a skeleton has restarted while a
    /// Proxy was connected to its shared memory region.
    SkeletonMockedMemoryFixture& WithAlreadyConnectedProxy();

    void ExpectServiceUsageMarkerFileCreatedOrOpenedAndClosed() noexcept;

    ServiceDataControl CreateServiceDataControlWithEvent(ElementFqId element_fq_id, QualityType quality_type) noexcept;
    EventControl& GetEventControlFromServiceDataControl(ElementFqId element_fq_id,
                                                        ServiceDataControl& service_data_control) noexcept;

    template <typename SampleType>
    ServiceDataStorage CreateServiceDataStorageWithEvent(ElementFqId element_fq_id) noexcept
    {
        ServiceDataStorage service_data_storage{data_shared_memory_resource_mock_->getMemoryResourceProxy()};

        auto* event_data_storage = data_shared_memory_resource_mock_->construct<EventDataStorage<SampleType>>(
            10U, data_shared_memory_resource_mock_->getMemoryResourceProxy());

        auto inserted_data_slots = service_data_storage.events_.emplace(
            std::piecewise_construct, std::forward_as_tuple(element_fq_id), std::forward_as_tuple(event_data_storage));
        EXPECT_TRUE(inserted_data_slots.second);

        const DataTypeMetaInfo sample_meta_info{10U, 16U};
        auto* event_data_raw_array = event_data_storage->data();
        auto inserted_meta_info = service_data_storage.events_metainfo_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(element_fq_id),
            std::forward_as_tuple(sample_meta_info, event_data_raw_array));
        EXPECT_TRUE(inserted_meta_info.second);

        return service_data_storage;
    }

    template <typename SampleType>
    EventDataStorage<SampleType>& GetEventStorageFromServiceDataStorage(
        ElementFqId element_fq_id,
        ServiceDataStorage& service_data_storage) noexcept
    {
        auto event_data_storage_it = service_data_storage.events_.find(element_fq_id);
        EXPECT_NE(event_data_storage_it, service_data_storage.events_.cend());
        auto event_data_storage_offset_ptr = event_data_storage_it->second;

        auto* const typed_event_data_storage =
            event_data_storage_offset_ptr.template get<EventDataStorage<SampleType>>();
        EXPECT_NE(typed_event_data_storage, nullptr);
        return *typed_event_data_storage;
    }

    void CleanUpSkeleton();

    ::testing::NiceMock<impl::RuntimeMock> runtime_mock_{};
    ::testing::NiceMock<lola::RuntimeMock> lola_runtime_mock_{};
    os::MockGuard<::testing::NiceMock<os::FcntlMock>> fcntl_mock_{};
    os::MockGuard<::testing::NiceMock<os::StatMock>> stat_mock_{};
    os::MockGuard<::testing::NiceMock<os::UnistdMock>> unistd_mock_{};
    ::testing::NiceMock<filesystem::FilesystemFactoryFake> filesystem_fake_{};
    ::testing::NiceMock<impl::tracing::TracingRuntimeMock> tracing_runtime_mock_{};
    ::testing::NiceMock<impl::tracing::mock_binding::TracingRuntime> binding_tracing_runtime_mock_{};
    ::testing::NiceMock<MessagePassingServiceMock> message_passing_mock_{};

    ::testing::NiceMock<memory::shared::SharedMemoryFactoryMock> shared_memory_factory_mock_{};
    ::testing::NiceMock<ShmPathBuilderMock> shm_path_builder_mock_{};
    ::testing::NiceMock<PartialRestartPathBuilderMock> partial_restart_path_builder_mock_{};

    std::shared_ptr<::testing::NiceMock<memory::shared::SharedMemoryResourceHeapAllocatorMock>>
        control_qm_shared_memory_resource_mock_{
            std::make_shared<::testing::NiceMock<memory::shared::SharedMemoryResourceHeapAllocatorMock>>(
                test::kControlQmMemoryResourceId)};
    std::shared_ptr<::testing::NiceMock<memory::shared::SharedMemoryResourceHeapAllocatorMock>>
        control_asil_b_shared_memory_resource_mock_{
            std::make_shared<::testing::NiceMock<memory::shared::SharedMemoryResourceHeapAllocatorMock>>(
                test::kControlAsilBMemoryResourceId)};
    std::shared_ptr<::testing::NiceMock<memory::shared::SharedMemoryResourceHeapAllocatorMock>>
        data_shared_memory_resource_mock_{
            std::make_shared<::testing::NiceMock<memory::shared::SharedMemoryResourceHeapAllocatorMock>>(
                test::kDataMemoryResourceId)};

    // Since these objects rely on the default behaviour of some mocks (e.g. the mocked lola Runtime), we create them
    // after setting the default mock behaviours in the body of the constructor.
    std::unique_ptr<ServiceDataControl> service_data_control_qm_{nullptr};
    std::unique_ptr<ServiceDataControl> service_data_control_asil_b_{nullptr};
    std::unique_ptr<ServiceDataStorage> service_data_storage_{nullptr};

    std::unique_ptr<Skeleton> skeleton_{nullptr};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_TEST_RESOURCES_H
