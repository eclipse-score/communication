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
#include "score/filesystem/error.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/methods/method_data.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/containers/non_relocatable_vector.h"
#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/fake/my_bounded_shared_memory_resource.h"
#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/shared_memory_resource_mock.h"
#include "score/memory/shared/user_permission.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/assert_support.hpp>
#include <score/stop_token.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <sys/types.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \brief This file contains tests related to how a lola/Proxy handles ProxyMethods. Tests for lola/ProxyMethod are in
/// proxy_method_test.cpp.

namespace score::mw::com::impl::lola
{

namespace
{

using namespace ::testing;

using memory::DataTypeSizeInfo;

constexpr auto kMethodChannelPrefix{"/lola-methods-0000000000000002-00003-06543-"};

const std::string kDummyMethodName0{"my_dummy_method_0"};
const std::string kDummyMethodName1{"my_dummy_method_1"};
const std::string kDummyMethodName2{"my_dummy_method_2"};

constexpr LolaMethodId kDummyMethodId0{10U};
constexpr LolaMethodId kDummyMethodId1{11U};
constexpr LolaMethodId kDummyMethodId2{12U};

constexpr LolaMethodInstanceDeployment::QueueSize kDummyQueueSize0{5U};
constexpr LolaMethodInstanceDeployment::QueueSize kDummyQueueSize1{6U};
constexpr LolaMethodInstanceDeployment::QueueSize kDummyQueueSize2{7U};

LolaServiceId kLolaServiceId{2U};
LolaServiceInstanceId::InstanceId kLolaInstanceId{3U};

const LolaServiceInstanceDeployment kLolaServiceInstanceDeploymentWithMethods{
    kLolaInstanceId,
    {},
    {},
    {{kDummyMethodName0, LolaMethodInstanceDeployment{kDummyQueueSize0}},
     {kDummyMethodName1, LolaMethodInstanceDeployment{kDummyQueueSize1}},
     {kDummyMethodName2, LolaMethodInstanceDeployment{kDummyQueueSize2}}}};
const LolaServiceTypeDeployment kLolaServiceTypeDeploymentWithMethods{
    kLolaServiceId,
    {},
    {},
    {{kDummyMethodName0, kDummyMethodId0}, {kDummyMethodName1, kDummyMethodId1}, {kDummyMethodName2, kDummyMethodId2}}};

const ConfigurationStore kConfigurationStore{InstanceSpecifier::Create(std::string{"my_instance_spec"}).value(),
                                             make_ServiceIdentifierType("foo"),
                                             QualityType::kASIL_B,
                                             kLolaServiceTypeDeploymentWithMethods,
                                             kLolaServiceInstanceDeploymentWithMethods};

const std::optional<DataTypeSizeInfo> kEmptyInArgsTypeErasedDataInfo{};
const std::optional<DataTypeSizeInfo> kEmptyReturnTypeTypeErasedDataInfo{};
const DataTypeSizeInfo kValidInArgsTypeErasedDataInfo{16U, 16U};
const DataTypeSizeInfo kValidReturnTypeTypeErasedDataInfo{32U, 8U};

const DataTypeSizeInfo kValidInArgsTypeErasedDataInfo2{24U, 8U};
const DataTypeSizeInfo kValidReturnTypeTypeErasedDataInfo2{32U, 16U};

const std::optional<TypeErasedCallQueue::TypeErasedElementInfo> kEmptyTypeErasedInfo{};

class ProxyMethodHandlingFixture : public ProxyMockedMemoryFixture
{
  public:
    ProxyMethodHandlingFixture()
    {
        // When the proxy checks if the shared memory region already exists within
        // SetupMethods(), by default, the memory region should not exist.
        ON_CALL(filesystem_fake_.GetStandard(), Exists(StartsWith(kMethodChannelPrefix))).WillByDefault(Return(false));
    }

    ProxyMethodHandlingFixture& GivenAProxy()
    {
        InitialiseProxyWithConstructor(kConfigurationStore.GetInstanceIdentifier());
        SCORE_LANGUAGE_FUTURECPP_ASSERT(proxy_ != nullptr);
        return *this;
    }

    ProxyMethodHandlingFixture& GivenAMockedSharedMemoryResource()
    {
        mock_method_memory_resource_ = std::make_shared<memory::shared::SharedMemoryResourceMock>();
        ON_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _))
            .WillByDefault(Return(mock_method_memory_resource_));
        return *this;
    }

    ProxyMethodHandlingFixture& GivenAFakeSharedMemoryResource()
    {
        ON_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _))
            .WillByDefault(WithArgs<1, 2>(Invoke([this](auto initialize_callback, auto user_space_to_reserve) {
                fake_method_memory_resource_ =
                    std::make_shared<memory::shared::test::MyBoundedSharedMemoryResource>(user_space_to_reserve);
                std::invoke(initialize_callback, fake_method_memory_resource_);
                return fake_method_memory_resource_;
            })));
        return *this;
    }

    ProxyMethodHandlingFixture& WithRegisteredProxyMethods(
        std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>
            methods_to_register)
    {
        for (auto& [method_id, type_erased_element_info] : methods_to_register)
        {
            const ElementFqId element_fq_id{kLolaServiceId, method_id, kLolaInstanceId, ServiceElementType::METHOD};
            proxy_method_storage_.emplace_back(*proxy_, element_fq_id, type_erased_element_info);
        }
        return *this;
    }

    const MethodData& GetMethodDataFromShm()
    {
        const auto* const base_address = fake_method_memory_resource_->getUsableBaseAddress();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(base_address != nullptr);
        const auto* const method_data = static_cast<const MethodData*>(base_address);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(method_data != nullptr);
        return *method_data;
    }

    score::cpp::stop_source stop_source_;
    std::shared_ptr<memory::shared::SharedMemoryResourceMock> mock_method_memory_resource_{nullptr};
    std::shared_ptr<memory::shared::test::MyBoundedSharedMemoryResource> fake_method_memory_resource_{nullptr};

    /// Although we generally prefer to use the Facade pattern to manage mocks which must be handed over to the class
    /// under test using a unique_ptr, we don't own the StandardFilesystemMock and so cannot easily introduce a facade.
    std::unique_ptr<filesystem::StandardFilesystemMock> standard_filesystem_mock_owner_{
        std::make_unique<filesystem::StandardFilesystemMock>()};

    containers::NonRelocatableVector<ProxyMethod> proxy_method_storage_{5U};
};

TEST_F(ProxyMethodHandlingFixture, EnablingZeroMethodsDoesNotCreateSharedMemory)
{
    // Given that no ProxyMethods were registered
    GivenAProxy().GivenAMockedSharedMemoryResource();

    // Expecting that no shared memory region will be created
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _)).Times(0);

    // When calling SetupMethods with an empty enabled_method_names vector
    const auto result = proxy_->SetupMethods({});

    // Then no error is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyMethodHandlingFixture, EnablingMethodsWithoutArgsOrReturnTypesDoesNotCreateSharedMemory)
{
    // Given that 2 ProxyMethods with no in args or return types were registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0, kEmptyTypeErasedInfo}, {kDummyMethodId1, kEmptyTypeErasedInfo}});

    // Expecting that no shared memory region will be created
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _)).Times(0);

    // When calling SetupMethods with the names of the two registered ProxyMethods
    const auto result = proxy_->SetupMethods({kDummyMethodName0, kDummyMethodName1});

    // Then no error is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyMethodHandlingFixture, SuccessfullyCreatingSharedMemoryReturnsSuccess)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that the shared memory creation succeeds
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _))
        .WillOnce(Return(mock_method_memory_resource_));

    // When calling SetupMethods with the name of the registered ProxyMethod
    const auto result = proxy_->SetupMethods({kDummyMethodName0});

    // Then no error is returned
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyMethodHandlingFixture, FailingToCreateSharedMemoryReturnsError)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that the shared memory creation fails and returns a nullptr
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _))
        .WillOnce(Return(nullptr));

    // When calling SetupMethods with the name of the registered ProxyMethod
    const auto result = proxy_->SetupMethods({kDummyMethodName0});

    // Then an error is returned
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(ProxyMethodHandlingFixture, CreatesMethodCallQueueForEachMethodInShm)
{
    // Given that 2 ProxyMethods are registered
    GivenAProxy().GivenAFakeSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}},
         {kDummyMethodId1,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo2, kValidReturnTypeTypeErasedDataInfo2, kDummyQueueSize2}}});

    // When calling SetupMethods with the name of the registered ProxyMethod
    score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0, kDummyMethodName1});

    // Then a MethodData object will be created which contains TypeErasedCallQueues
    // for each method
    const auto& method_data = GetMethodDataFromShm();
    ASSERT_EQ(method_data.method_call_queues_.size(), 2U);
    EXPECT_THAT(method_data.method_call_queues_.at(0).first, kDummyMethodId0);
    EXPECT_THAT(method_data.method_call_queues_.at(1).first, kDummyMethodId1);
}

TEST_F(ProxyMethodHandlingFixture, SetsInArgsAndReturnStoragesForEachMethodInShm)
{
    // Given that 2 ProxyMethods are registered
    GivenAProxy().GivenAFakeSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}},
         {kDummyMethodId1,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo2, kValidReturnTypeTypeErasedDataInfo2, kDummyQueueSize2}}});

    // When calling SetupMethods with the name of the registered ProxyMethod
    score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0, kDummyMethodName1});

    // Then the SetInArgsAndReturnStorages will be set for each method (which we validate by checking whether the method
    // can allocate InArgs without crashing, since the allocation is using the inserted storages)
    for (auto& method : proxy_method_storage_)
    {
        score::cpp::ignore = method.AllocateInArgs(0);
    }
}

TEST_F(ProxyMethodHandlingFixture, CreatesSharedMemoryWithUserPermissionsContainingSkeletonApplicationId)
{
    // Given that a ProxyMethod is registered which is connected to a Fake ServiceDataStorage which stores kDummyUid as
    // the UID of the skeleton (check the construction of FakeMockedServiceData in the constructor of
    // ProxyMockedMemoryFixture)
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that the shared memory creation is called with read and write permissions for the skeleton's
    // uid
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _))
        .WillOnce(
            Invoke(WithArg<3>([this](auto user_permissions) -> std::shared_ptr<memory::shared::ISharedMemoryResource> {
                const auto* const user_permissions_map =
                    std::get_if<memory::shared::permission::UserPermissionsMap>(&user_permissions);
                EXPECT_NE(user_permissions_map, nullptr);
                if (user_permissions_map == nullptr)
                {
                    return nullptr;
                }
                EXPECT_EQ(user_permissions_map->size(), 2U);
                EXPECT_THAT(*user_permissions_map,
                            Contains(Pair<score::os::Acl::Permission, std::vector<uid_t>>(os::Acl::Permission::kRead,
                                                                                        {kDummyUid})));
                EXPECT_THAT(*user_permissions_map,
                            Contains(Pair<score::os::Acl::Permission, std::vector<uid_t>>(os::Acl::Permission::kWrite,
                                                                                        {kDummyUid})));

                return mock_method_memory_resource_;
            })));

    // When calling SetupMethods with the name of the registered ProxyMethod
    score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0});
}

using ProxySetupMethodsPartialRestartFixture = ProxyMethodHandlingFixture;
TEST_F(ProxySetupMethodsPartialRestartFixture, RemovesStaleArtefactsIfShmFileAlreadyExists)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that we check if the shm file already exists in the filesystem
    // which returns that it already exists
    EXPECT_CALL(filesystem_fake_.GetStandard(), Exists(StartsWith(kMethodChannelPrefix))).WillOnce(Return(true));

    // Expecting that RemoveStaleArtefacts will be called with the same shm path
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, RemoveStaleArtefacts(StartsWith(kMethodChannelPrefix)));

    // When calling SetupMethods with the name of the registered ProxyMethod
    score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0});
}

TEST_F(ProxySetupMethodsPartialRestartFixture, ReturnsErrorWhenCheckingIfShmFileAlreadyExistsReturnsError)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that we check if the shm file already exists in the filesystem which returns an error
    EXPECT_CALL(filesystem_fake_.GetStandard(), Exists(StartsWith(kMethodChannelPrefix)))
        .WillOnce(Return(MakeUnexpected(filesystem::ErrorCode::kCouldNotRetrieveStatus)));

    // When calling SetupMethods with the name of the registered ProxyMethod
    const auto result = proxy_->SetupMethods({kDummyMethodName0});

    // Then an error is returned
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

using ProxySetupMethodsMessagePassingFixture = ProxyMethodHandlingFixture;
TEST_F(ProxySetupMethodsMessagePassingFixture, MethodsWithArgsOrReturnTypesCallsSubscribeServiceMethod)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that SubscribeServiceMethod will be called
    EXPECT_CALL(*mock_service_, SubscribeServiceMethod(_))
        .WillOnce(Invoke([](auto skeleton_instance_identifier) -> ResultBlank {
            // Then SubscribeServiceMethod is called with a
            // SkeletonInstanceIdentifier taking values from the configuration
            EXPECT_EQ(skeleton_instance_identifier.service_id, kLolaServiceId);
            EXPECT_EQ(skeleton_instance_identifier.instance_id, kLolaInstanceId);
            return {};
        }));

    // When calling SetupMethods with the name of the registered ProxyMethod
    score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0});
}

TEST_F(ProxySetupMethodsMessagePassingFixture, MethodsWithArgsOrReturnTypesForwardsErrorFromSubscribeServiceMethod)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that SubscribeServiceMethod will be called which returns an error
    const auto call_service_method_subscribed_error_code = ComErrc::kCallQueueFull;
    EXPECT_CALL(*mock_service_, SubscribeServiceMethod(_)).WillOnce(Invoke([](auto) -> ResultBlank {
        return MakeUnexpected(call_service_method_subscribed_error_code);
    }));

    // When calling SetupMethods with the name of the registered ProxyMethod
    const auto setup_methods_result = proxy_->SetupMethods({kDummyMethodName0});

    // Then the result contains the error returned by message passing
    ASSERT_FALSE(setup_methods_result.has_value());
    EXPECT_EQ(setup_methods_result.error(), call_service_method_subscribed_error_code);
}

TEST_F(ProxySetupMethodsMessagePassingFixture, EnablingZeroMethodsDoesNotNotifiesSubscribeServiceMethod)
{
    // Given that no ProxyMethods were registered
    GivenAProxy().GivenAMockedSharedMemoryResource();

    // Expecting that SubscribeServiceMethod will not be called
    EXPECT_CALL(*mock_service_, SubscribeServiceMethod(_)).Times(0);

    // When calling SetupMethods with an empty enabled_method_names vector
    score::cpp::ignore = proxy_->SetupMethods({});
}

TEST_F(ProxySetupMethodsMessagePassingFixture, MethodsWithoutArgsOrReturnTypesForwardsErrorFromSubscribeServiceMethod)
{
    // Given that 2 ProxyMethods with no in args or return types were registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0, kEmptyTypeErasedInfo}, {kDummyMethodId1, kEmptyTypeErasedInfo}});

    // Expecting that SubscribeServiceMethod will be called which returns an error
    const auto call_service_method_subscribed_error_code = ComErrc::kCallQueueFull;
    EXPECT_CALL(*mock_service_, SubscribeServiceMethod(_)).WillOnce(Invoke([](auto) -> ResultBlank {
        return MakeUnexpected(call_service_method_subscribed_error_code);
    }));

    // When calling SetupMethods with the names of the two registered ProxyMethods
    const auto setup_methods_result = proxy_->SetupMethods({kDummyMethodName0, kDummyMethodName1});

    // Then the result contains the error returned by message passing
    ASSERT_FALSE(setup_methods_result.has_value());
    EXPECT_EQ(setup_methods_result.error(), call_service_method_subscribed_error_code);
}

TEST_F(ProxySetupMethodsMessagePassingFixture, EnablingMethodsWithoutArgsOrReturnTypesNotifiesServiceMethodSubscribed)
{
    // Given that 2 ProxyMethods with no in args or return types were registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0, kEmptyTypeErasedInfo}, {kDummyMethodId1, kEmptyTypeErasedInfo}});

    // Expecting that SubscribeServiceMethod will be called
    EXPECT_CALL(*mock_service_, SubscribeServiceMethod(_))
        .WillOnce(Invoke([](auto skeleton_instance_identifier) -> ResultBlank {
            // Then SubscribeServiceMethod is called with a
            // SkeletonInstanceIdentifier taking values from the configuration
            EXPECT_EQ(skeleton_instance_identifier.service_id, kLolaServiceId);
            EXPECT_EQ(skeleton_instance_identifier.instance_id, kLolaInstanceId);
            return {};
        }));

    // When calling SetupMethods with the names of the two registered ProxyMethods
    score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0, kDummyMethodName1});
}

TEST_F(ProxySetupMethodsMessagePassingFixture, FailingToGetLolaRuntimeTerminates)
{
    // Given that a ProxyMethod is registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0,
          TypeErasedCallQueue::TypeErasedElementInfo{
              kValidInArgsTypeErasedDataInfo, kValidReturnTypeTypeErasedDataInfo, kDummyQueueSize0}}});

    // Expecting that GetBindingRuntime is called on the impl runtime which returns
    // a nullptr
    EXPECT_CALL(runtime_mock_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa)).WillOnce(Return(nullptr));

    // When calling SetupMethods with the name of the registered ProxyMethod
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0}));
}

class ProxySetupMethodsShmSizeParamaterizedFixture
    : public ProxyMethodHandlingFixture,
      public ::testing::WithParamInterface<
          std::pair<std::vector<std::string_view>,
                    std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>>
{
};

INSTANTIATE_TEST_SUITE_P(
    ProxySetupMethodsShmSizeParamaterizedFixture,
    ProxySetupMethodsShmSizeParamaterizedFixture,
    ::testing::Values(

        // Single method containing InArgs and Return Type
        std::make_pair<std::vector<std::string_view>,
                       std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>(
            {kDummyMethodName0},
            {std::make_pair(
                kDummyMethodId0,
                std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                    {std::optional<DataTypeSizeInfo>{{24, 8}}, std::optional<DataTypeSizeInfo>{{32, 16}}, 5U}})}),

        // Multiple methods containing InArgs and Return Type
        std::make_pair<std::vector<std::string_view>,
                       std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>(
            {kDummyMethodName0, kDummyMethodName1},
            {std::make_pair(
                 kDummyMethodId0,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 8}}, std::optional<DataTypeSizeInfo>{{32, 16}}, 3U}}),
             std::make_pair(
                 kDummyMethodId1,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 16}}, std::optional<DataTypeSizeInfo>{{104, 8}}, 4U}})}),

        // Multiple methods containing InArgs and Return Type with different padding
        // to previous test (The actual location of the padding will be determined
        // by the size of MethodData and its elements which are allocated before the
        // InArgs / Return types. However, the amount of padding between Method0 and
        // Method1 will be different to the test above).
        std::make_pair<std::vector<std::string_view>,
                       std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>(
            {kDummyMethodName0, kDummyMethodName1},
            {std::make_pair(
                 kDummyMethodId0,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{24, 8}}, std::optional<DataTypeSizeInfo>{{32, 16}}, 4U}}),
             std::make_pair(
                 kDummyMethodId1,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 16}}, std::optional<DataTypeSizeInfo>{{104, 8}}, 6U}})}),

        // Method with empty InArgs
        std::make_pair<std::vector<std::string_view>,
                       std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>(
            {kDummyMethodName0, kDummyMethodName1},
            {std::make_pair(
                 kDummyMethodId0,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 8}}, std::optional<DataTypeSizeInfo>{{32, 16}}, 3U}}),
             std::make_pair(kDummyMethodId1,
                            std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                                {kEmptyInArgsTypeErasedDataInfo, std::optional<DataTypeSizeInfo>{{104, 8}}, 5U}})}),

        // Method with empty Return type
        std::make_pair<std::vector<std::string_view>,
                       std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>(
            {kDummyMethodName0, kDummyMethodName1},
            {std::make_pair(kDummyMethodId0,
                            std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                                {std::optional<DataTypeSizeInfo>{{32, 8}},
                                 kEmptyReturnTypeTypeErasedDataInfo,
                                 7U}}),  // Adjust if needed based on actual structure
             std::make_pair(
                 kDummyMethodId1,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 16}}, std::optional<DataTypeSizeInfo>{{104, 8}}, 8U}})}),

        // Method with empty InArg and Return type (this method will be ignored in
        // size calculations)
        std::make_pair<std::vector<std::string_view>,
                       std::vector<std::pair<LolaMethodId, std::optional<TypeErasedCallQueue::TypeErasedElementInfo>>>>(
            {kDummyMethodName0, kDummyMethodName1, kDummyMethodName2},
            {std::make_pair(
                 kDummyMethodId0,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 8}}, std::optional<DataTypeSizeInfo>{{32, 16}}, 3U}}),
             std::make_pair(kDummyMethodId1, kEmptyTypeErasedInfo),
             std::make_pair(
                 kDummyMethodId2,
                 std::optional<TypeErasedCallQueue::TypeErasedElementInfo>{
                     {std::optional<DataTypeSizeInfo>{{32, 16}}, std::optional<DataTypeSizeInfo>{{104, 8}}, 5U}})})));

/// Note. This test assumes that the allocation behaviour of
/// fake_method_memory_resource_ (i.e. MyBoundedSharedMemoryResource) behaves the
/// same as SharedMemoryResource. It currently uses the same allocation algorithm
/// (i.e. detail::do_allocation_algorithm) and the tracking of allocated memory is
/// implemented that same (this must be verified by inspection). We will also have
/// component tests which will check that the size calculation is correct with the
/// real SharedMemoryResource.
TEST_P(ProxySetupMethodsShmSizeParamaterizedFixture, AllocatesEveryByteInSpecifiedShmSize)
{
    // Given that 2 ProxyMethods with various in args / return types
    const auto& [method_names, methods_to_register] = GetParam();
    GivenAProxy().GivenAFakeSharedMemoryResource().WithRegisteredProxyMethods(methods_to_register);

    // Expecting that a shared memory region will be created with the calculated shm
    // size
    std::size_t actual_shm_size{0U};
    EXPECT_CALL(shared_memory_factory_mock_guard_.mock_, Create(StartsWith(kMethodChannelPrefix), _, _, _, _))
        .WillOnce(Invoke(WithArgs<1, 2>([this, &actual_shm_size](auto initialize_callback, auto shm_size) {
            fake_method_memory_resource_ =
                std::make_shared<memory::shared::test::MyBoundedSharedMemoryResource>(shm_size);
            std::invoke(initialize_callback, fake_method_memory_resource_);
            actual_shm_size = shm_size;
            return fake_method_memory_resource_;
        })));

    // When calling SetupMethods with the names of the registered ProxyMethods
    score::cpp::ignore = proxy_->SetupMethods(method_names);

    // Then the number of bytes allocated should equal the size that the shm region
    // was created with
    EXPECT_EQ(fake_method_memory_resource_->GetUserAllocatedBytes(), actual_shm_size);
}

TEST_F(ProxyMethodHandlingFixture, EnablingMethodsThatWereNotRegisteredTerminates)
{
    // Given that a ProxyMethod was registered
    GivenAProxy().GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods(
        {{kDummyMethodId0, kEmptyTypeErasedInfo}});

    // When calling SetupMethods with a ProxyMethod name which does not correspond
    // to the registered ProxyMethod Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName1}));
}

TEST_F(ProxyMethodHandlingFixture, EnablingMethodsThatAreNotInTheConfigurationTerminates)
{
    GivenAProxy().GivenAMockedSharedMemoryResource();

    // When calling SetupMethods with a ProxyMethod name which does not exist in the
    // configuration Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = proxy_->SetupMethods({"SomeInvalidMethodName"}));
}

TEST_F(ProxyMethodHandlingFixture, EnablingMethodThatDoesNotContainQueueSizeInConfigurationTerminates)
{
    // Given a configuration which contains a LolaMethodInstanceDeployment with
    // empty queue size
    const std::optional<LolaMethodInstanceDeployment::QueueSize> empty_queue_size{};
    const LolaServiceInstanceDeployment lola_service_instance_deployment_missing_queue_size{
        kLolaInstanceId, {}, {}, {{kDummyMethodName0, LolaMethodInstanceDeployment{empty_queue_size}}}};
    const LolaServiceTypeDeployment lola_service_type_deployment{
        kLolaServiceId, {}, {}, {{kDummyMethodName0, kDummyMethodId0}}};

    const ConfigurationStore configuration_store{InstanceSpecifier::Create(std::string{"my_instance_spec"}).value(),
                                                 make_ServiceIdentifierType("foo"),
                                                 QualityType::kASIL_B,
                                                 lola_service_type_deployment,
                                                 lola_service_instance_deployment_missing_queue_size};

    // Given a proxy that was created from the configuration missing the queue size
    InitialiseProxyWithCreate(configuration_store.GetInstanceIdentifier());
    SCORE_LANGUAGE_FUTURECPP_ASSERT(proxy_ != nullptr);

    GivenAMockedSharedMemoryResource().WithRegisteredProxyMethods({{kDummyMethodId0, kEmptyTypeErasedInfo}});

    // When calling SetupMethods with a ProxyMethod name which corresponds to the
    // registered ProxyMethod Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = proxy_->SetupMethods({kDummyMethodName0}));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
