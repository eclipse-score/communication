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
#include "score/mw/com/impl/bindings/lola/skeleton.h"

#include "score/filesystem/factory/filesystem_factory_fake.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/runtime.h"
#include "score/os/mman.h"
#include "score/os/mocklib/acl_mock.h"

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using TestSampleType = std::uint8_t;

#if defined(__QNXNTO__)
constexpr auto data_shm = "/dev/shmem/lola-data-0000000000000001-00016";
constexpr auto control_shm = "/dev/shmem/lola-ctl-0000000000000001-00016";
constexpr auto asil_control_shm = "/dev/shmem/lola-ctl-0000000000000001-00016-b";
#else
constexpr auto data_shm = "/dev/shm/lola-data-0000000000000001-00016";
constexpr auto control_shm = "/dev/shm/lola-ctl-0000000000000001-00016";
constexpr auto asil_control_shm = "/dev/shm/lola-ctl-0000000000000001-00016-b";
#endif

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
constexpr std::size_t kNumberOfSlots{10U};

SkeletonBinding::SkeletonEventBindings kEmptyEvents{};
SkeletonBinding::SkeletonFieldBindings kEmptyFields{};

std::size_t GetSize(const std::string& file_path)
{
    struct stat data{};
    const auto result = stat(file_path.c_str(), &data);
    if (result == 0 && data.st_size > 0)
    {
        return static_cast<std::size_t>(data.st_size);
    }

    return 0;
}

bool IsWriteableForOwner(const std::string& filePath)
{
    struct stat data{};
    const auto result = stat(filePath.c_str(), &data);
    if (result == 0)
    {
        return data.st_mode & S_IWUSR;
    }

    std::cerr << "File does not exist" << std::endl;
    std::abort();
}

bool IsWriteableForOthers(const std::string& filePath)
{
    struct stat data{};
    const auto result = stat(filePath.c_str(), &data);
    if (result == 0)
    {
        bool group_write_permission = data.st_mode & S_IWGRP;
        bool others_write_permission = data.st_mode & S_IWOTH;
        return group_write_permission || others_write_permission;
    }

    std::cerr << "File does not exist" << std::endl;
    std::abort();
}

struct EventInfo
{
    std::size_t event_size;
    std::size_t max_samples;
};

std::size_t CalculateLowerBoundControlShmSize(const std::vector<EventInfo>& events)
{
    std::size_t lower_bound{sizeof(ServiceDataControl)};
    for (const auto event_info : events)
    {
        lower_bound += sizeof(decltype(ServiceDataControl::event_controls_)::value_type);
        lower_bound += event_info.max_samples * sizeof(EventDataControl::EventControlSlots::value_type);
    }
    return lower_bound;
}

std::size_t CalculateLowerBoundDataShmSize(const std::vector<EventInfo>& events)
{
    std::size_t lower_bound{sizeof(ServiceDataStorage)};
    for (const auto event_info : events)
    {
        lower_bound += sizeof(decltype(ServiceDataStorage::events_)::value_type);
        lower_bound += event_info.max_samples * event_info.event_size;
        lower_bound += sizeof(decltype(ServiceDataStorage::events_metainfo_)::value_type);
    }
    return lower_bound;
}

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;

/// \brief Test fixture for lola::Skeleton tests, which are generally based on "real" shared-mem access.
class SkeletonComponentTestFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        impl::Runtime::InjectMock(&runtime_mock_);
        EXPECT_CALL(runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillRepeatedly(::testing::Return(&lola_runtime_mock_));
    }

    void TearDown() override
    {
        score::memory::shared::MemoryResourceRegistry::getInstance().clear();
        score::filesystem::IStandardFilesystem::instance().Remove("/tmp/lola-data-0000000000000001-00016_lock");
        score::filesystem::IStandardFilesystem::instance().Remove("/tmp/lola-ctl-0000000000000001-00016_lock");
        score::filesystem::IStandardFilesystem::instance().Remove("/tmp/lola-ctl-0000000000000001-00016-b_lock");

        score::filesystem::IStandardFilesystem::instance().Remove(data_shm);
        score::filesystem::IStandardFilesystem::instance().Remove(control_shm);
        score::filesystem::IStandardFilesystem::instance().Remove(asil_control_shm);

        EXPECT_FALSE(fileExists(data_shm));
        EXPECT_FALSE(fileExists(control_shm));
        EXPECT_FALSE(fileExists(asil_control_shm));

        score::memory::shared::MemoryResourceRegistry::getInstance().clear();
        impl::Runtime::InjectMock(nullptr);
    }

    std::unique_ptr<Skeleton> CreateSkeleton(
        const InstanceIdentifier& instance_identifier,
        score::filesystem::Filesystem filesystem = filesystem::FilesystemFactory{}.CreateInstance()) noexcept
    {
        auto shm_path_builder = std::make_unique<ShmPathBuilder>(test::kLolaServiceId);
        auto partial_restart_path_builder = std::make_unique<PartialRestartPathBuilder>(test::kLolaServiceId);

        auto unit = Skeleton::Create(
            instance_identifier, filesystem, std::move(shm_path_builder), std::move(partial_restart_path_builder));
        return unit;
    }

    SkeletonComponentTestFixture& WithAServiceInstanceDeploymentContainingSingleEventAndField(
        const QualityType quality_type,
        score::cpp::optional<std::size_t> configured_shared_memory_size = {})
    {
        events_.emplace(test::kFooEventName, mock_event_binding_);
        lola_event_instance_deployments_.push_back(
            {test::kFooEventName, LolaEventInstanceDeployment{kNumberOfSlots, 10U, 1U, true, 0}});
        fields_.emplace(test::kFooFieldName, mock_field_binding_);
        lola_field_instance_deployments_.push_back(
            {test::kFooFieldName, LolaEventInstanceDeployment{kNumberOfSlots, 10U, 1U, true, 0}});
        service_instance_deployment_ = std::make_unique<ServiceInstanceDeployment>(
            test::kFooService,
            CreateLolaServiceInstanceDeployment(test::kDefaultLolaInstanceId,
                                                lola_event_instance_deployments_,
                                                lola_field_instance_deployments_,
                                                {},
                                                {},
                                                configured_shared_memory_size),
            quality_type,
            kInstanceSpecifier);
        return *this;
    }

    SkeletonComponentTestFixture& WithAServiceTypeDeploymentContainingSingleEventAndField()
    {
        service_type_deployment_ = std::make_unique<ServiceTypeDeployment>(CreateTypeDeployment(
            1U, {{test::kFooEventName, test::kFooEventId}}, {{test::kFooFieldName, test::kFooFieldId}}));
        return *this;
    }

    InstanceIdentifier CreateInstanceIdentifier()
    {
        EXPECT_NE(service_instance_deployment_, nullptr);
        EXPECT_NE(service_type_deployment_, nullptr);
        return make_InstanceIdentifier(*service_instance_deployment_, *service_type_deployment_);
    }

    /// mocks used by test
    impl::RuntimeMock runtime_mock_{};
    lola::RuntimeMock lola_runtime_mock_{};

    mock_binding::SkeletonEvent<TestSampleType> mock_event_binding_{};
    mock_binding::SkeletonEvent<TestSampleType> mock_field_binding_{};

    std::vector<std::pair<std::string, LolaEventInstanceDeployment>> lola_event_instance_deployments_;
    std::vector<std::pair<std::string, LolaFieldInstanceDeployment>> lola_field_instance_deployments_;

    std::unique_ptr<ServiceInstanceDeployment> service_instance_deployment_{nullptr};
    std::unique_ptr<ServiceTypeDeployment> service_type_deployment_{nullptr};

    SkeletonBinding::SkeletonEventBindings events_{};
    SkeletonBinding::SkeletonFieldBindings fields_{};
};

TEST_F(SkeletonComponentTestFixture, ACLPermissionsSetCorrectly)
{
    RecordProperty("Verifies", "SCR-5899184");
    RecordProperty("Description", "Ensure that the correct ACLs are set that are configured.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid instance identifier and constructed unit
    const auto instance_identifier = GetValidASILInstanceIdentifierWithACL();

    // from which we create our UoT
    auto unit = CreateSkeleton(instance_identifier);
    ASSERT_NE(unit, nullptr);

    // Expecting that the ACL Levels are set correctly for the QM and ASIL split segments
    score::os::MockGuard<score::os::AclMock> acl_mock{};

    EXPECT_CALL(*acl_mock, acl_add_perm(_, score::os::Acl::Permission::kRead)).Times(4);
    EXPECT_CALL(*acl_mock, acl_add_perm(_, score::os::Acl::Permission::kWrite)).Times(2);
    EXPECT_CALL(*acl_mock,
                acl_set_qualifier(_,
                                  ::testing::MatcherCast<const void*>(::testing::SafeMatcherCast<const uint32_t*>(
                                      ::testing::Pointee(::testing::Eq(42))))))
        .Times(3);
    EXPECT_CALL(*acl_mock,
                acl_set_qualifier(_,
                                  ::testing::MatcherCast<const void*>(::testing::SafeMatcherCast<const uint32_t*>(
                                      ::testing::Pointee(::testing::Eq(43))))))
        .Times(3);

    // When preparing to offer a service
    unit->PrepareOffer(kEmptyEvents, kEmptyFields, {});
}

TEST_F(SkeletonComponentTestFixture, CannotCreateTheSameSkeletonTwice)
{
    RecordProperty("Verifies", "SCR-5898312, SCR-5898324");  // SWS_CM_00102, SWS_CM_10450
    RecordProperty("Description", "Tries to offer the same service twice");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto filesystem = filesystem::FilesystemFactory{}.CreateInstance();

    // Given a valid instance identifier
    const auto instance_identifier = GetValidInstanceIdentifier();

    // from which we create our UoT
    auto unit = CreateSkeleton(instance_identifier, filesystem);
    ASSERT_NE(unit, nullptr);

    auto second_unit = CreateSkeleton(instance_identifier, filesystem);
    ASSERT_EQ(second_unit, nullptr);
}

/// \brief Test verifies, that the skeleton, when created from a valid InstanceIdentifier, creates the expected
/// shared-memory objects.
/// \details In this case - as the deployment contained in the valid InstanceIdentifier defines only QM - we expect one
/// data and one control shm-object for QM and NO shm-object for ASIL-B!
TEST_F(SkeletonComponentTestFixture, ShmObjectsAreCreated)
{
    // SWS_CM_00700
    RecordProperty("Verifies",
                   "SCR-5897992, SCR-5899052, SCR-5899136, SCR-5899143, SCR-5899159, SCR-5899160, SCR-5899126, "
                   "SCR-5899059, 2908703");
    RecordProperty("Description",
                   "Ensure that QM Control segment and Data segment are created. Maximum memory allocation is "
                   "configured on runtime and allocated on offer. Thus, it is ensured that "
                   "enough resources are available after subscribe.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    score::os::Mman::restore_instance();

    // Given a valid instance identifier of an QM only instance
    const auto instance_identifier = GetValidInstanceIdentifier();

    // from which we create our UoT
    auto unit = CreateSkeleton(instance_identifier);
    ASSERT_NE(unit, nullptr);

    // When offering the service
    auto result = unit->PrepareOffer(kEmptyEvents, kEmptyFields, {});

    // Then this PrepareOffer succeeds
    EXPECT_TRUE(result.has_value());

    // Then the respective Shared Memory file for data is created
    EXPECT_TRUE(fileExists(data_shm));
    EXPECT_FALSE(IsWriteableForOthers(data_shm));
    EXPECT_TRUE(IsWriteableForOwner(data_shm));

    // .... and the respective Shared Memory file for QM control is created
    EXPECT_TRUE(fileExists(control_shm));
    // ... and the control shm-object is writeable for others
    // (our instance_identifier is based on a deployment without ACLs)
    EXPECT_TRUE(IsWriteableForOthers(control_shm));
    EXPECT_TRUE(IsWriteableForOwner(control_shm));

    // .... and NO Shared Memory file for control for ASIL-B is created
    EXPECT_FALSE(fileExists(asil_control_shm));

    // and we expect, that the size of the shm-data file is at least test::kConfiguredDeploymentShmSize as the
    // instance_identifier had a configured shm-size test::kConfiguredDeploymentShmSize.
    EXPECT_GT(GetSize(data_shm), test::kConfiguredDeploymentShmSize);
}

/// \brief Test verifies, that the skeleton, when created from a valid InstanceIdentifier defining an ASIL-B enabled
/// service, creates also the expected ASIL-B shared-memory object for control.
/// \details Thios test is basically an extension to the test "ShmObjectsAreCreated" above!
TEST_F(SkeletonComponentTestFixture, ASILShmIsCreated)
{
    RecordProperty("Verifies", "SCR-5899059, SCR-5899136, SCR-5899143, SCR-5899159, SCR-5899160, 2908703");
    RecordProperty("Description", "Ensure that ASIL Control segment is created");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid instance identifier
    const auto instance_identifier = GetValidASILInstanceIdentifier();

    // from which we create our UoT
    auto unit = CreateSkeleton(instance_identifier);
    ASSERT_NE(unit, nullptr);

    // When offering the service
    auto result = unit->PrepareOffer(kEmptyEvents, kEmptyFields, {});
    EXPECT_TRUE(result.has_value());

    // Then the respective Shared Memory file is created
    EXPECT_TRUE(fileExists(asil_control_shm));
    // ... and the control shm-object is writeable for others
    // (our instance_identifier is based on a deployment without ACLs)
    EXPECT_TRUE(IsWriteableForOthers(asil_control_shm));
}

TEST_F(SkeletonComponentTestFixture, DataShmObjectSizeCalc_Simulation_QM)
{
    RecordProperty("Verifies", "SCR-5899126");
    RecordProperty("Description", "Check if the data_shm is calculated correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a skeleton with one event "fooEvent" and one field "fooField" registered
    WithAServiceInstanceDeploymentContainingSingleEventAndField(QualityType::kASIL_QM)
        .WithAServiceTypeDeploymentContainingSingleEventAndField();
    const auto instance_identifier = CreateInstanceIdentifier();

    auto unit = CreateSkeleton(instance_identifier);
    ASSERT_NE(unit, nullptr);

    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&test::kValidMinimalTypeDeployment.binding_info_);
    ASSERT_NE(lola_service_type_deployment, nullptr);

    // Expect, that the LoLa runtime returns that ShmSize calculation shall be done via simulation
    EXPECT_CALL(lola_runtime_mock_, GetShmSizeCalculationMode()).WillOnce(Return(ShmSizeCalculationMode::kSimulation));

    // Expecting that the event and field are offered during the simulation dry run
    ON_CALL(mock_event_binding_, PrepareOffer())
        .WillByDefault(testing::Invoke([&unit, lola_service_type_deployment]() -> ResultBlank {
            ElementFqId event_fqn{lola_service_type_deployment->service_id_,
                                  test::kFooEventId,
                                  test::kDefaultLolaInstanceId,
                                  ServiceElementType::EVENT};
            unit->Register<uint8_t>(event_fqn, test::kDefaultEventProperties);
            return {};
        }));
    ON_CALL(mock_field_binding_, PrepareOffer())
        .WillByDefault(testing::Invoke([&unit, lola_service_type_deployment]() -> ResultBlank {
            ElementFqId event_fqn{lola_service_type_deployment->service_id_,
                                  test::kFooFieldId,
                                  test::kDefaultLolaInstanceId,
                                  ServiceElementType::FIELD};
            unit->Register<uint8_t>(event_fqn, test::kDefaultEventProperties);
            return {};
        }));

    // When offering a service and all events
    const auto val = unit->PrepareOffer(events_, fields_, {});
    mock_event_binding_.PrepareOffer();
    mock_field_binding_.PrepareOffer();

    // then expect, that it has a value!
    EXPECT_TRUE(val.has_value());

    // Then the respective Shared Memory file for Data is created with a size larger than already the pure payload
    // within data-shm-object would occupy (this is a lower bound for consistency)
    EXPECT_GE(GetSize(data_shm), CalculateLowerBoundDataShmSize({{sizeof(TestSampleType), kNumberOfSlots}}));

    // Then the respective Shared Memory file for Control is created with a size larger than already the pure payload
    // within control-shm-object would occupy (this is a lower bound for consistency)
    EXPECT_GE(GetSize(control_shm), CalculateLowerBoundControlShmSize({{sizeof(TestSampleType), kNumberOfSlots}}));
}

TEST_F(SkeletonComponentTestFixture, DataShmObjectSizeCalc_Simulation_AsilB)
{
    RecordProperty("Verifies", "SCR-5899126");
    RecordProperty("Description", "Check if the data_shm is calculated correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a skeleton with one event "fooEvent" and one field "fooField" registered
    WithAServiceInstanceDeploymentContainingSingleEventAndField(QualityType::kASIL_B)
        .WithAServiceTypeDeploymentContainingSingleEventAndField();
    const auto instance_identifier = CreateInstanceIdentifier();

    auto unit = CreateSkeleton(instance_identifier);
    ASSERT_NE(unit, nullptr);

    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&test::kValidMinimalTypeDeployment.binding_info_);
    ASSERT_NE(lola_service_type_deployment, nullptr);

    // Expect, that the LoLa runtime returns that ShmSize calculation shall be done via simulation
    EXPECT_CALL(lola_runtime_mock_, GetShmSizeCalculationMode()).WillOnce(Return(ShmSizeCalculationMode::kSimulation));

    // Expecting that the event and field are offered during the simulation dry run
    ON_CALL(mock_event_binding_, PrepareOffer())
        .WillByDefault(testing::Invoke([&unit, lola_service_type_deployment]() -> ResultBlank {
            ElementFqId event_fqn{lola_service_type_deployment->service_id_,
                                  test::kFooEventId,
                                  test::kDefaultLolaInstanceId,
                                  ServiceElementType::EVENT};
            unit->Register<uint8_t>(event_fqn, test::kDefaultEventProperties);
            return {};
        }));
    ON_CALL(mock_field_binding_, PrepareOffer())
        .WillByDefault(testing::Invoke([&unit, lola_service_type_deployment]() -> ResultBlank {
            ElementFqId event_fqn{lola_service_type_deployment->service_id_,
                                  test::kFooFieldId,
                                  test::kDefaultLolaInstanceId,
                                  ServiceElementType::FIELD};
            unit->Register<uint8_t>(event_fqn, test::kDefaultEventProperties);
            return {};
        }));

    // When offering a service and all events
    const auto val = unit->PrepareOffer(events_, fields_, {});
    mock_event_binding_.PrepareOffer();
    mock_field_binding_.PrepareOffer();

    // then expect, that it has a value!
    EXPECT_TRUE(val.has_value());

    // Then the respective Shared Memory file for Data is created with a size larger than already the pure payload
    // within data-shm-object would occupy (this is a lower bound for consistency)
    EXPECT_GE(GetSize(data_shm), CalculateLowerBoundDataShmSize({{sizeof(TestSampleType), kNumberOfSlots}}));

    // Then the respective Shared Memory file for Control is created with a size larger than already the pure payload
    // within control-shm-object would occupy (this is a lower bound for consistency) for both the QM and asil b
    // sections
    EXPECT_GE(GetSize(control_shm), CalculateLowerBoundControlShmSize({{sizeof(TestSampleType), kNumberOfSlots}}));
    EXPECT_GE(GetSize(asil_control_shm), CalculateLowerBoundControlShmSize({{sizeof(TestSampleType), kNumberOfSlots}}));
}

TEST_F(SkeletonComponentTestFixture,
       DataShmObjectSizeCalc_Simulation_QM_DoesNotTerminateWhenConfiguredSizeIsLargerThanEstimate)
{
    RecordProperty("Verifies", "SCR-5899126");
    RecordProperty("Description", "Check if the data_shm is calculated correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // At the time of writing, 648 bytes is needed for the data segment used in this test.
    constexpr std::size_t large_enough_user_specified_memory_size{1000U};

    // Given a skeleton with one event "fooEvent" and one field "fooField" registered with a user configured shared
    // memory size which is larger than the required data shm size
    WithAServiceInstanceDeploymentContainingSingleEventAndField(QualityType::kASIL_QM,
                                                                large_enough_user_specified_memory_size)
        .WithAServiceTypeDeploymentContainingSingleEventAndField();
    const auto instance_identifier = CreateInstanceIdentifier();

    auto unit = CreateSkeleton(instance_identifier);
    ASSERT_NE(unit, nullptr);

    const auto* const lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&test::kValidMinimalTypeDeployment.binding_info_);
    ASSERT_NE(lola_service_type_deployment, nullptr);

    // and that the LoLa runtime returns that ShmSize calculation shall be done via simulation
    EXPECT_CALL(lola_runtime_mock_, GetShmSizeCalculationMode()).WillOnce(Return(ShmSizeCalculationMode::kSimulation));

    // When preparing to offer a service
    const auto prepare_offer_result = unit->PrepareOffer(events_, fields_, {});
    mock_event_binding_.PrepareOffer();
    mock_field_binding_.PrepareOffer();

    // then expect, that it has a value!
    EXPECT_TRUE(prepare_offer_result.has_value());
}

using SkeletonComponentTestDeathTest = SkeletonComponentTestFixture;
TEST_F(SkeletonComponentTestDeathTest, DataShmObjectSizeCalc_Simulation_QM_TerminatesWithTooSmallConfiguredSize)
{
    RecordProperty("Verifies", "SCR-5899126");
    RecordProperty("Description", "Check if the data_shm is calculated correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    auto test_function = [this] {
        constexpr std::size_t too_small_user_specified_memory_size{0U};

        // Given a skeleton with one event "fooEvent" and one field "fooField" registered with a user configured shared
        // memory size which is smaller than the required data shm size
        WithAServiceInstanceDeploymentContainingSingleEventAndField(QualityType::kASIL_QM,
                                                                    too_small_user_specified_memory_size)
            .WithAServiceTypeDeploymentContainingSingleEventAndField();
        const auto instance_identifier = CreateInstanceIdentifier();

        auto unit = CreateSkeleton(instance_identifier);
        ASSERT_NE(unit, nullptr);

        const auto* const lola_service_type_deployment =
            std::get_if<LolaServiceTypeDeployment>(&test::kValidMinimalTypeDeployment.binding_info_);
        ASSERT_NE(lola_service_type_deployment, nullptr);

        // and that the LoLa runtime returns that ShmSize calculation shall be done via simulation
        EXPECT_CALL(lola_runtime_mock_, GetShmSizeCalculationMode())
            .WillOnce(Return(ShmSizeCalculationMode::kSimulation));

        // When preparing to offer a service
        score::cpp::ignore = unit->PrepareOffer(events_, fields_, {});
    };
    // Then the program terminates
    EXPECT_DEATH(test_function(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
