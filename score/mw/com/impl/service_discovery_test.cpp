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
#include "score/mw/com/impl/service_discovery.h"

#include "score/mw/com/impl/bindings/lola/runtime_mock.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"

#include "score/result/result.h"

#include <score/optional.hpp>
#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using namespace ::testing;

class DestructorNotifier
{
  public:
    explicit DestructorNotifier() noexcept : handler_destruction_barrier_{std::make_unique<std::promise<void>>()} {}

    ~DestructorNotifier() noexcept
    {
        if (handler_destruction_barrier_ != nullptr)
        {
            handler_destruction_barrier_->set_value();
        }
    }

    std::future<void> GetFuture() const noexcept
    {
        return handler_destruction_barrier_->get_future();
    }

    DestructorNotifier(DestructorNotifier&& other) noexcept
        : handler_destruction_barrier_{other.handler_destruction_barrier_.release()}
    {
        other.handler_destruction_barrier_ = nullptr;
    }

    DestructorNotifier(const DestructorNotifier&) = delete;
    DestructorNotifier& operator=(const DestructorNotifier&) = delete;
    DestructorNotifier& operator=(DestructorNotifier&&) = delete;

  private:
    std::unique_ptr<std::promise<void>> handler_destruction_barrier_;
};

class ServiceDiscoveryTest : public Test
{
  protected:
    void SetUp() override
    {
        ON_CALL(runtime_, GetBindingRuntime(_)).WillByDefault(Return(&lola_runtime_));

        ON_CALL(lola_runtime_, GetServiceDiscoveryClient()).WillByDefault(ReturnRef(service_discovery_client_));

        ON_CALL(service_discovery_client_, StartFindService(_, _, _)).WillByDefault(Return(ResultBlank{}));

        ON_CALL(service_discovery_client_, StopFindService(_)).WillByDefault(Return(ResultBlank{}));
    }

    ServiceDiscoveryTest& WithAServiceContainingOneInstances() noexcept
    {
        const auto service_identfier = make_ServiceIdentifierType("/bla/blub/service1");
        const auto quality_type = QualityType::kASIL_QM;
        const LolaServiceId lola_service_id{2U};
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{1U});

        ON_CALL(runtime_, resolve(instance_specifier_))
            .WillByDefault(Return(std::vector<InstanceIdentifier>{config_stores_[0].GetInstanceIdentifier()}));

        return *this;
    }

    ServiceDiscoveryTest& WithAServiceContainingTwoInstances() noexcept
    {
        const auto service_identfier = make_ServiceIdentifierType("/bla/blub/service1");
        const auto quality_type = QualityType::kASIL_QM;
        const LolaServiceId lola_service_id{2U};
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{1U});
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{2U});

        ON_CALL(runtime_, resolve(instance_specifier_))
            .WillByDefault(Return(std::vector<InstanceIdentifier>{config_stores_[0].GetInstanceIdentifier(),
                                                                  config_stores_[1].GetInstanceIdentifier()}));

        return *this;
    }

    ServiceDiscoveryTest& WithAServiceContainingFourInstances() noexcept
    {
        const auto service_identfier = make_ServiceIdentifierType("/bla/blub/service1");
        const auto quality_type = QualityType::kASIL_QM;
        const LolaServiceId lola_service_id{2U};
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{1U});
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{2U});
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{3U});
        config_stores_.emplace_back(
            instance_specifier_, service_identfier, quality_type, lola_service_id, LolaServiceInstanceId{4U});

        ON_CALL(runtime_, resolve(instance_specifier_))
            .WillByDefault(Return(std::vector<InstanceIdentifier>{config_stores_[0].GetInstanceIdentifier(),
                                                                  config_stores_[1].GetInstanceIdentifier(),
                                                                  config_stores_[2].GetInstanceIdentifier(),
                                                                  config_stores_[3].GetInstanceIdentifier()}));

        return *this;
    }

    std::vector<ConfigurationStore> config_stores_;

    RuntimeMock runtime_{};
    lola::RuntimeMock lola_runtime_{};
    ServiceDiscoveryClientMock service_discovery_client_{};

    InstanceSpecifier instance_specifier_{InstanceSpecifier::Create("/bla/blub/specifier").value()};

    std::unique_ptr<ServiceDiscovery> unit_ = std::make_unique<ServiceDiscovery>(runtime_);
};

using ServiceDiscoveryFindServiceInstanceSpecifierFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryFindServiceInstanceSpecifierFixture,
       FindServiceReturnsHandlesForEachIdentifierIfBindingReturnsNoErrors)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description", "FindService can find a service using an instance specifier.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient and a service which contains 2 instances
    WithAServiceContainingTwoInstances();

    // Expecting FindService is called for both instances which both return valid handles
    const auto expected_handle_1 = config_stores_[0].GetHandle();
    const auto expected_handle_2 = config_stores_[1].GetHandle();
    ON_CALL(service_discovery_client_, FindService(config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillByDefault(Return(ServiceHandleContainer<HandleType>{expected_handle_1}));
    ON_CALL(service_discovery_client_, FindService(config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillByDefault(Return(ServiceHandleContainer<HandleType>{expected_handle_2}));

    // When finding a service instance using an instance specifier
    auto handles_result = unit_->FindService(instance_specifier_);

    // Then handles for both service instances are returned
    ASSERT_TRUE(handles_result.has_value());
    const auto& handles = handles_result.value();
    ASSERT_EQ(handles.size(), 2);
    EXPECT_THAT(handles, Contains(expected_handle_1));
    EXPECT_THAT(handles, Contains(expected_handle_2));
}

TEST_F(ServiceDiscoveryFindServiceInstanceSpecifierFixture,
       FindServiceReturnsHandlesOnlyForIdentifiersWithNoBindingErrors)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description", "FindService can find a service using an instance specifier.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient and a service which contains 2 instances
    WithAServiceContainingTwoInstances();

    // Expecting FindService for both instances but only the second one returns a valid handle
    const auto expected_handle_2 = config_stores_[1].GetHandle();
    ON_CALL(service_discovery_client_, FindService(config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillByDefault(Return(Unexpected{ComErrc::kErroneousFileHandle}));
    ON_CALL(service_discovery_client_, FindService(config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillByDefault(Return(ServiceHandleContainer<HandleType>{expected_handle_2}));

    // When finding a service instance using an instance specifier
    auto handles_result = unit_->FindService(instance_specifier_);

    // Then a handle for the service instance that was found is returned
    ASSERT_TRUE(handles_result.has_value());
    const auto& handles = handles_result.value();
    ASSERT_EQ(handles.size(), 1);
    EXPECT_THAT(handles, Contains(expected_handle_2));
}

TEST_F(ServiceDiscoveryFindServiceInstanceSpecifierFixture, FindServiceShouldReturnErrorIfBindingReturnsOnlyErrors)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description",
                   "FindService returns a kBindingFailure error code if binding returns an error for all instances.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient and a service which contains 2 instances
    WithAServiceContainingTwoInstances();

    // Expecting that errors are returned for all instances by the binding
    const auto binding_error_code = ComErrc::kErroneousFileHandle;
    EXPECT_CALL(service_discovery_client_, FindService(config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(Unexpected{binding_error_code}));
    EXPECT_CALL(service_discovery_client_, FindService(config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(Unexpected{binding_error_code}));

    // When finding a service instance using an instance specifier
    auto handles_result = unit_->FindService(instance_specifier_);

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());
    const auto expected_error_code = ComErrc::kBindingFailure;
    EXPECT_EQ(handles_result.error(), expected_error_code);
}

using ServiceDiscoveryFindServiceInstanceSpecifierDeathTest = ServiceDiscoveryFindServiceInstanceSpecifierFixture;
TEST_F(ServiceDiscoveryFindServiceInstanceSpecifierDeathTest, FindServiceTerminatesIfNoInstancesCanBeResolved)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description", "FindService terminates if InstanceSpecifier cannot be resolved.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient and a service which contains 0 instances

    // Expecting that no instances are resolved from the InstanceSpecifier
    ON_CALL(runtime_, resolve(instance_specifier_)).WillByDefault(Return(std::vector<InstanceIdentifier>{}));

    // Expecting that FindService will never be called on the binding
    EXPECT_CALL(service_discovery_client_, FindService(_)).Times(0);

    // When finding a service instance using an instance specifier
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit_->FindService(instance_specifier_), ".*");
}

TEST_F(ServiceDiscoveryFindServiceInstanceSpecifierDeathTest, FindServiceTerminatesIfBindingRuntimeIsNull)
{
    // Given a ServiceDiscoveryClient and a service which contains 1 instance
    WithAServiceContainingOneInstances();

    // Expecting that GetBindingRuntime will return nullptr on inavalid binding_type
    ON_CALL(runtime_, GetBindingRuntime(_)).WillByDefault(Return(nullptr));

    // When finding a service instance
    // Then the program terminates
    EXPECT_DEATH(
        { score::cpp::ignore = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_); }, ".*");
}

using ServiceDiscoveryFindServiceInstanceIdentifierFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryFindServiceInstanceIdentifierFixture, FindServiceReturnsHandleIfBindingReturnsNoError)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description", "FindService can find a service using an instance identifier.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient and a service which contains 2 instances
    WithAServiceContainingTwoInstances();

    // Expecting that a single service instance is found
    auto expected_handle = config_stores_[0].GetHandle();
    EXPECT_CALL(service_discovery_client_, FindService(config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(ServiceHandleContainer<HandleType>{expected_handle}));

    // When finding for this service instance via an instance identifier
    auto handles_result = unit_->FindService(config_stores_[0].GetInstanceIdentifier());

    // Then the service instance is found
    ASSERT_TRUE(handles_result.has_value());
    EXPECT_EQ(handles_result.value().size(), 1);
}

TEST_F(ServiceDiscoveryFindServiceInstanceIdentifierFixture,
       FindServiceForInstanceIdentifierReturnsErrorIfBindingReturnsError)
{
    RecordProperty("ParentRequirement", "SCR-14005977, SCR-14110930, SCR-18804932");
    RecordProperty("Description",
                   "FindService returns an empty handles containing if binding cannot find any instances.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscoveryClient and a service which contains 2 instances
    WithAServiceContainingTwoInstances();

    // Expecting that a an error is returned by the binding
    EXPECT_CALL(service_discovery_client_, FindService(config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(Unexpected(ComErrc::kErroneousFileHandle)));

    // When finding for this service instance via an instance identifier
    auto handles_result = unit_->FindService(config_stores_[0].GetInstanceIdentifier());

    // Then an error is returned
    ASSERT_FALSE(handles_result.has_value());
    const auto expected_error_code = ComErrc::kBindingFailure;
    EXPECT_EQ(handles_result.error(), expected_error_code);
}

using ServiceDiscoveryStartFindServiceInstanceSpecifierFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture,
       StartFindServiceCallsBindingSpecificStartFindServiceForEachIdentifer)
{
    // @todo: Right now this is only testing the Lola binding, no other. Can be extended in future.
    RecordProperty("ParentRequirement", "SCR-21792392, SCR-21788845");
    RecordProperty("Description", "All instance identifiers specific to a Instance specifier are called.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    WithAServiceContainingTwoInstances();

    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()));
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[1].GetEnrichedInstanceIdentifier()));

    unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture, StartFindServiceReturnsHandleIfSuccessful)
{
    auto handle = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
    EXPECT_TRUE(handle.has_value());
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture,
       StartFindServiceCallsBindingSpecificStopFindServiceOnFailure)
{
    InSequence seq{};
    FindServiceHandle handle1{make_FindServiceHandle(0)};
    FindServiceHandle handle2{make_FindServiceHandle(0)};
    FindServiceHandle handle3{make_FindServiceHandle(0)};
    FindServiceHandle handle4{make_FindServiceHandle(0)};

    // Given a ServiceDiscovery with a configuration containing three instances.
    WithAServiceContainingFourInstances();

    // Expecting that StartFindService will be called on each instance, with the last one returning an error
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle1), Return(score::cpp::blank{})));
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle2), Return(score::cpp::blank{})));
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[2].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle3), Return(score::cpp::blank{})));
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[3].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle4), Return(Unexpected{ComErrc::kBindingFailure})));

    // and expecting that StopFindService is called for each instance
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle1))));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle2))));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle3))));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle4))));

    // When StartFindService is called
    unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture,
       StartFindServiceReturnsErrorFromBindingStartFindServiceWhenBindingStartAndStopFindServiceReturnsErrors)
{
    // Given a ServiceDiscovery with a service containing 1 instance
    WithAServiceContainingOneInstances();

    // and given that binding calls both StopFindService and StartFindService return errors.
    const auto start_find_service_error = ComErrc::kBindingFailure;
    ON_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillByDefault(Return(Unexpected{start_find_service_error}));

    const auto stop_find_service_error = ComErrc::kCommunicationLinkError;
    ON_CALL(service_discovery_client_, StopFindService(_)).WillByDefault(Return(Unexpected{stop_find_service_error}));

    // When StartFindService is called.
    auto result = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);

    // Then error from StartFindService binding call is returned.
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), start_find_service_error);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture,
       StartFindServiceDoesNotCallBindingSpecificStartFindServiceAfterFirstBindingFailure)
{
    InSequence seq{};
    FindServiceHandle handle1{make_FindServiceHandle(0)};
    FindServiceHandle handle2{make_FindServiceHandle(0)};

    // Given a ServiceDiscovery with a configuration containing three instances.
    WithAServiceContainingFourInstances();

    // Expecting that StartFindService will be called on each instance, with the second one returning an error
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle1), Return(score::cpp::blank{})));
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle2), Return(Unexpected{ComErrc::kBindingFailure})));
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[2].GetEnrichedInstanceIdentifier()))
        .Times(0);
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[3].GetEnrichedInstanceIdentifier()))
        .Times(0);

    // and expecting that StopFindService is called only for the first and second instances
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle1))));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle2))));
    EXPECT_CALL(service_discovery_client_, StopFindService(_)).Times(0);

    // When StartFindService is called
    unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture, StartFindServiceForwardsCorrectHandler)
{
    WithAServiceContainingTwoInstances();

    ON_CALL(service_discovery_client_, StartFindService(_, _, _)).WillByDefault([](auto handle, auto handler, auto) {
        handler({}, handle);
        return ResultBlank{};
    });

    std::int32_t value{0};
    unit_->StartFindService(
        [&value](auto, auto) noexcept {
            value++;
        },
        instance_specifier_);

    EXPECT_EQ(value, 2);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture, StartFindServiceCanNotFindACallback)
{
    // Given a ServiceDiscovery with a service containing 1 instance
    WithAServiceContainingOneInstances();

    // When StopFindService is called before the StartFindSercice callback has a chance to execute
    ON_CALL(service_discovery_client_, StartFindService(_, _, _))
        .WillByDefault([this](auto handle, auto handler, auto) {
            score::cpp::ignore = this->unit_->StopFindService(handle);
            handler({}, handle);
            return ResultBlank{};
        });

    std::int32_t value{7};
    // Where the callback would modify an observable
    auto handle = unit_->StartFindService(
        [&value](auto, auto) noexcept {
            value++;
        },
        instance_specifier_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT(handle.has_value());

    // Then the observable shoud stay unmodified
    EXPECT_EQ(value, 7);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture, StartFindServiceReturnsWorkingHandle)
{
    WithAServiceContainingTwoInstances();

    FindServiceHandle handle_1{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle_1), Return(ResultBlank{})));
    EXPECT_CALL(service_discovery_client_,
                StartFindService(Eq(ByRef(handle_1)), _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(ResultBlank{}));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle_1)))).Times(2);

    auto start_result = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
    ASSERT_TRUE(start_result.has_value());
    auto stop_result = unit_->StopFindService(start_result.value());
    EXPECT_TRUE(stop_result.has_value());
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture,
       StartFindServiceWillUseTheSameFindServiceHandleForAllFoundInstanceIdentifiers)
{
    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingTwoInstances();

    // Expecting that StartFindService will be called on the binding for each InstanceIdentifier
    score::cpp::optional<FindServiceHandle> find_service_handle_1{};
    score::cpp::optional<FindServiceHandle> find_service_handle_2{};
    ON_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillByDefault(DoAll(SaveArg<0>(&find_service_handle_1), Return(ResultBlank{})));
    ON_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillByDefault(DoAll(SaveArg<0>(&find_service_handle_2), Return(ResultBlank{})));

    // When calling StartFindService with an InstanceSpecifier and a handler
    auto returned_find_service_handle = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);

    // Then the same handler should be used for each call to StartFindService on the binding
    ASSERT_TRUE(find_service_handle_1.has_value());
    ASSERT_TRUE(find_service_handle_2.has_value());
    EXPECT_EQ(find_service_handle_1.value(), find_service_handle_2.value());
    EXPECT_EQ(find_service_handle_1.value(), returned_find_service_handle.value());
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceSpecifierFixture,
       StartFindServiceWillStoreRegisteredReceiveHandlerWithGeneratedHandle)
{
    RecordProperty("ParentRequirement", "SCR-20236346");
    RecordProperty("Description",
                   "Checks that the registered FindServiceHandler is stored when StartFindService with an "
                   "InstanceSpecifier is called. Since the handler is move-only, it will only be stored in one place.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::promise<void> handler_destruction_barrier{};

    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // and a FindServiceHandler which contains a DestructorNotifier object which will set the value of the
    // handler_destruction_barrier promise on destruction
    DestructorNotifier destructor_notifier{};
    const auto destructor_notifier_future = destructor_notifier.GetFuture();
    auto find_service_handler = [destructor_notifier = std::move(destructor_notifier)](auto, auto) noexcept {};

    // When calling StartFindService with an InstanceSpecifier and the handler
    score::cpp::ignore = unit_->StartFindService(std::move(find_service_handler), instance_specifier_);

    // Then the handler should not have been destroyed by StartFindService, indicating that the handler has been stored
    // internally (since it's move-only)
    const auto future_status = destructor_notifier_future.wait_for(std::chrono::milliseconds{1U});
    EXPECT_EQ(future_status, std::future_status::timeout);
}

using ServiceDiscoveryStartFindServiceInstanceIdentifierFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture, StartFindServiceCallsBindingSpecificStartFindService)
{
    WithAServiceContainingTwoInstances();

    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()));

    unit_->StartFindService([](auto, auto) noexcept {}, config_stores_[0].GetInstanceIdentifier());
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture, StartFindServiceReturnsHandleIfSuccessful)
{
    WithAServiceContainingTwoInstances();

    auto handle = unit_->StartFindService([](auto, auto) noexcept {}, config_stores_[0].GetInstanceIdentifier());
    EXPECT_TRUE(handle.has_value());
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture,
       StartFindServiceCallsStopFindServiceIfBindingSpecificStartFindServiceFailed)
{
    WithAServiceContainingTwoInstances();

    InSequence seq{};
    FindServiceHandle handle_1{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle_1), Return(Unexpected{ComErrc::kBindingFailure})));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle_1))));

    auto error = unit_->StartFindService([](auto, auto) noexcept {}, config_stores_[0].GetInstanceIdentifier());
    ASSERT_FALSE(error.has_value());
    EXPECT_EQ(error.error(), ComErrc::kBindingFailure);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture,
       StartFindServiceReturnsErrorFromBindingStartFindServiceWhenBindingStartAndStopFindServiceReturnsErrors)
{
    // Given a ServiceDiscovery with a service containing 1 instance
    WithAServiceContainingOneInstances();

    // and given that binding calls to both StopFindService and StartFindService return errors.
    const auto start_find_service_error = ComErrc::kBindingFailure;
    ON_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillByDefault(Return(Unexpected{start_find_service_error}));

    const auto stop_find_service_error = ComErrc::kCommunicationLinkError;
    ON_CALL(service_discovery_client_, StopFindService(_)).WillByDefault(Return(Unexpected{stop_find_service_error}));

    // When StartFindService is called
    auto result =
        unit_->StartFindService([](auto, auto) noexcept {}, config_stores_[0].GetEnrichedInstanceIdentifier());

    // Then error from StartFindService binding call is returned.
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), start_find_service_error);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture, StartFindServiceForwardsCorrectHandler)
{
    WithAServiceContainingTwoInstances();

    ON_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillByDefault([](auto handle, auto handler, auto) {
            handler({}, handle);
            return ResultBlank{};
        });

    bool value{false};
    unit_->StartFindService(
        [&value](auto, auto) noexcept {
            value = true;
        },
        config_stores_[0].GetInstanceIdentifier());

    EXPECT_TRUE(value);
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture, StartFindServiceReturnsWorkingHandle)
{
    WithAServiceContainingTwoInstances();

    FindServiceHandle handle_1{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle_1), Return(ResultBlank{})));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle_1))));

    auto start_result = unit_->StartFindService([](auto, auto) noexcept {}, config_stores_[0].GetInstanceIdentifier());
    ASSERT_TRUE(start_result.has_value());
    auto stop_result = unit_->StopFindService(start_result.value());
    EXPECT_TRUE(stop_result.has_value());
}

TEST_F(ServiceDiscoveryStartFindServiceInstanceIdentifierFixture,
       StartFindServiceWillStoreRegisteredReceiveHandlerWithGeneratedHandle)
{
    RecordProperty("ParentRequirement", "SCR-20236346");
    RecordProperty(
        "Description",
        "Checks that the registered FindServiceHandler is stored when StartFindService with an "
        "InstanceIdentifier is called. Since the handler is move-only, it will only be stored in one place.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // and a FindServiceHandler which contains a DestructorNotifier object which will set the value of the
    // handler_destruction_barrier promise on destruction
    DestructorNotifier destructor_notifier{};
    const auto destructor_notifier_future = destructor_notifier.GetFuture();
    auto find_service_handler = [destructor_notifier = std::move(destructor_notifier)](auto, auto) noexcept {};

    // When calling StartFindService with an InstanceSpecifier and the handler
    score::cpp::ignore =
        unit_->StartFindService(std::move(find_service_handler), config_stores_[0].GetInstanceIdentifier());

    // Then the handler should not have been destroyed by StartFindService, indicating that the handler has been stored
    // internally (since it's move-only)
    const auto future_status = destructor_notifier_future.wait_for(std::chrono::milliseconds{1U});
    EXPECT_EQ(future_status, std::future_status::timeout);
}

using ServiceDiscoveryStopFindServiceFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryStopFindServiceFixture, StopFindServiceInvokedIfForgottenByUser)
{
    WithAServiceContainingTwoInstances();

    FindServiceHandle handle_1{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle_1), Return(ResultBlank{})));
    EXPECT_CALL(service_discovery_client_,
                StartFindService(handle_1, _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(ResultBlank{}));
    EXPECT_CALL(service_discovery_client_, StopFindService(handle_1)).Times(2);

    auto start_result = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
    ASSERT_TRUE(start_result.has_value());
}

TEST_F(ServiceDiscoveryStopFindServiceFixture,
       StopFindServiceCallsBindingSpecificStopFindServiceForAllAssociatedInstanceIdentifiers)
{
    WithAServiceContainingTwoInstances();

    FindServiceHandle handle_1{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle_1), Return(ResultBlank{})));
    EXPECT_CALL(service_discovery_client_,
                StartFindService(Eq(ByRef(handle_1)), _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(ResultBlank{}));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle_1)))).Times(2);

    auto start_result = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
    ASSERT_TRUE(start_result.has_value());
    auto stop_result = unit_->StopFindService(start_result.value());
    EXPECT_TRUE(stop_result.has_value());
}

TEST_F(ServiceDiscoveryStopFindServiceFixture, StopFindServiceCallsBindingSpecificStopFindServiceEvenWhenOneFailed)
{
    WithAServiceContainingTwoInstances();

    FindServiceHandle handle_1{make_FindServiceHandle(0)};
    EXPECT_CALL(service_discovery_client_, StartFindService(_, _, config_stores_[0].GetEnrichedInstanceIdentifier()))
        .WillOnce(DoAll(SaveArg<0>(&handle_1), Return(ResultBlank{})));
    EXPECT_CALL(service_discovery_client_,
                StartFindService(handle_1, _, config_stores_[1].GetEnrichedInstanceIdentifier()))
        .WillOnce(Return(ResultBlank{}));
    EXPECT_CALL(service_discovery_client_, StopFindService(Eq(ByRef(handle_1))))
        .Times(2)
        .WillOnce(Return(Unexpected{ComErrc::kBindingFailure}));

    auto start_result = unit_->StartFindService([](auto, auto) noexcept {}, instance_specifier_);
    ASSERT_TRUE(start_result.has_value());
    auto stop_result = unit_->StopFindService(start_result.value());
    ASSERT_FALSE(stop_result.has_value());
    EXPECT_EQ(stop_result.error(), ComErrc::kBindingFailure);
}

TEST_F(ServiceDiscoveryStopFindServiceFixture, StopFindServiceWillDestroyRegisteredFindServiceHandler)
{
    RecordProperty("ParentRequirement", "SCR-20236346");
    RecordProperty(
        "Description",
        "Checks that the registered FindServiceHandler is destroyed when StopFindService is called. Since the "
        "handler is move-only, it will only have been stored in one place.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // and a FindServiceHandler which contains a DestructorNotifier object which will set the value of the
    // handler_destruction_barrier promise on destruction
    DestructorNotifier destructor_notifier{};
    const auto destructor_notifier_future = destructor_notifier.GetFuture();
    auto find_service_handler = [destructor_notifier = std::move(destructor_notifier)](auto, auto) noexcept {};

    // and given that StartFindService has been called with an InstanceSpecifier and the handler
    const auto find_service_handle = unit_->StartFindService(std::move(find_service_handler), instance_specifier_);

    // When calling StopFindService with the returned handle
    score::cpp::ignore = unit_->StopFindService(find_service_handle.value());

    // Then the handler passed to StartFindService should be destroyed
    destructor_notifier_future.wait();
}

TEST_F(ServiceDiscoveryStopFindServiceFixture, StopFindServiceSilentlyIgnoresSecondCallWithSameHandle)
{
    RecordProperty("ParentRequirement", "SCR-21792394");
    RecordProperty(
        "Description",
        "Checks that calling StopFindService multiple times with the same FindServiceHandle returns without an error.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // and a FindServiceHandler
    auto find_service_handler = [](auto, auto) noexcept {};

    // and given that StartFindService has been called with an InstanceSpecifier and the handler
    const auto find_service_handle = unit_->StartFindService(std::move(find_service_handler), instance_specifier_);

    // Expecting only a single call to StopFindService of the ServiceDiscoveryClient
    EXPECT_CALL(service_discovery_client_, StopFindService(_)).Times(1);

    // When calling StopFindService with the returned handle twice
    score::cpp::ignore = unit_->StopFindService(find_service_handle.value());
    auto result = unit_->StopFindService(find_service_handle.value());

    // Then the return value of the second call contains no error
    EXPECT_TRUE(result.has_value());
}

TEST_F(ServiceDiscoveryStopFindServiceFixture, CallingStopFindServiceInHandlerWillNotDestroyHandlerWhileItsBeingCalled)
{
    std::promise<void> handler_done{};

    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // Expecting that StartFindService is called on the binding and that we capture the start find service handler
    auto handler_promise = std::make_shared<std::promise<FindServiceHandler<HandleType>>>();
    auto handler_future = handler_promise->get_future();
    ON_CALL(service_discovery_client_, StartFindService(_, _, _))
        .WillByDefault(Invoke(WithArgs<1>([handler_promise](auto handler) {
            handler_promise->set_value(std::move(handler));
            return ResultBlank{};
        })));

    DestructorNotifier destructor_notifier{};
    auto find_service_handler = [this, &handler_done, destructor_notifier = std::move(destructor_notifier)](
                                    auto, auto find_service_handle) {
        // When calling StopFindService in the find service handler
        const auto result = unit_->StopFindService(find_service_handle);
        ASSERT_TRUE(result.has_value());

        // Then the handler passed to StartFindService should not be destroyed while the handler is still running
        const auto future_status = destructor_notifier.GetFuture().wait_for(std::chrono::seconds{1U});
        EXPECT_EQ(future_status, std::future_status::timeout);

        handler_done.set_value();
    };

    // When StartFindService is called
    const auto find_service_handle = unit_->StartFindService(std::move(find_service_handler), instance_specifier_);

    // and we wait for the handler to be captured
    auto captured_find_service_handler = handler_future.get();

    // and then we call the captured handler, simulating a service being found
    captured_find_service_handler(ServiceHandleContainer<HandleType>{config_stores_[0].GetHandle()},
                                  find_service_handle.value());

    // and then we wait for the handler to finish being called
    handler_done.get_future().wait();
}

TEST_F(ServiceDiscoveryStopFindServiceFixture, CallingStopFindServiceInHandlerWillDestroyHandlerOnceHandlerReturns)
{
    std::promise<void> handler_done{};

    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // Expecting that StartFindService is called on the binding and that we capture the start find service handler
    auto handler_promise = std::make_shared<std::promise<FindServiceHandler<HandleType>>>();
    auto handler_future = handler_promise->get_future();
    ON_CALL(service_discovery_client_, StartFindService(_, _, _))
        .WillByDefault(Invoke(WithArgs<1>([handler_promise](auto handler) {
            handler_promise->set_value(std::move(handler));
            return ResultBlank{};
        })));

    DestructorNotifier destructor_notifier{};
    const auto destructor_notifier_future = destructor_notifier.GetFuture();
    auto find_service_handler = [this, &handler_done, destructor_notifier = std::move(destructor_notifier)](
                                    auto, auto find_service_handle) {
        // When calling StopFindService in the find service handler
        const auto result = unit_->StopFindService(find_service_handle);
        ASSERT_TRUE(result.has_value());

        handler_done.set_value();
    };

    // When StartFindService is called
    const auto find_service_handle = unit_->StartFindService(std::move(find_service_handler), instance_specifier_);

    // and we wait for the handler to be captured
    auto captured_find_service_handler = handler_future.get();

    // and then we call the captured handler, simulating a service being found
    captured_find_service_handler(ServiceHandleContainer<HandleType>{config_stores_[0].GetHandle()},
                                  find_service_handle.value());

    // and then we wait for the handler to finish being called
    handler_done.get_future().wait();

    // Then the handler passed to StartFindService should be destroyed once the handler is done
    destructor_notifier_future.wait();
}

using ServiceDiscoveryDestructorFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryDestructorFixture, DestructorWillNotTerminateWhenStopFindServiceFailsOnShutdown)
{
    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // Expecting that StopFindService is called on the binding which returns an error
    const auto error_code = ComErrc::kBindingFailure;
    EXPECT_CALL(service_discovery_client_, StopFindService(_)).WillOnce(Return(Unexpected{error_code}));

    // given that StartFindService is called
    auto handle = unit_->StartFindService([](auto, auto) noexcept {}, config_stores_[0].GetInstanceIdentifier());
    ASSERT_TRUE(handle.has_value());

    // When we explicitly trigger the ServiceDiscovery destructor
    unit_.reset();

    // Then it does not terminate
}

using ServiceDiscoveryStopOfferServiceFixture = ServiceDiscoveryTest;
TEST_F(ServiceDiscoveryStopOfferServiceFixture, StopOfferServiceWithoutQualityTypeInvokesClientWithDefault)
{
    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // Expecting that StopOfferService is called on the binding with both quality types
    auto instance_id = config_stores_[0].GetInstanceIdentifier();
    EXPECT_CALL(service_discovery_client_, StopOfferService(instance_id, IServiceDiscovery::QualityTypeSelector::kBoth))
        .WillOnce(Return(ResultBlank{}));

    // When calling StopOfferService without specifying a quality type
    auto result = unit_->StopOfferService(instance_id);
    EXPECT_TRUE(result.has_value());
}

TEST_F(ServiceDiscoveryStopOfferServiceFixture, StopOfferServiceWithQualityTypeInvokesClientWithSpecifiedType)
{
    // Given a ServiceDiscovery with a mocked ServiceDiscoveryClient
    WithAServiceContainingOneInstances();

    // Expecting that StopOfferService is called on the binding with QM quality type
    auto instance_id = config_stores_[0].GetInstanceIdentifier();
    auto quality_type = IServiceDiscovery::QualityTypeSelector::kAsilQm;
    EXPECT_CALL(service_discovery_client_, StopOfferService(instance_id, quality_type)).WillOnce(Return(ResultBlank{}));

    // When calling StopOfferService with QM quality type
    auto result = unit_->StopOfferService(instance_id, quality_type);
    EXPECT_TRUE(result.has_value());
}
}  // namespace
}  // namespace score::mw::com::impl
