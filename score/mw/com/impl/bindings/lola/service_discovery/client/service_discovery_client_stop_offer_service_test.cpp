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
#include "score/mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_fixtures.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/mw/com/impl/i_service_discovery.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <memory>
#include <string>

namespace score::mw::com::impl::lola::test
{
namespace
{

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::DoDefault;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

const auto kOldFlagFileDirectory = GetServiceDiscoveryPath() / "1/1";
const auto kOldFlagFile = kOldFlagFileDirectory / "123456_asil-qm_1234";

constexpr auto kQmPathLabel{"asil-qm"};

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString = InstanceSpecifier::Create("/bla/blub/specifier").value();
ConfigurationStore kConfigStoreQm1{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{1U},
};
ConfigurationStore kConfigStoreQm2{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{2U},
};
ConfigurationStore kConfigStoreAsilB{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_B,
    kServiceId,
    LolaServiceInstanceId{3U},
};

using ServiceDiscoveryClientStopOfferFixture = ServiceDiscoveryClientFixture;
TEST_F(ServiceDiscoveryClientStopOfferFixture, HandlersAreCalledOnceWhenServiceIsStopOffered)
{
    RecordProperty("Verifies", "SCR-32385851");
    RecordProperty(
        "Description",
        "Checks that FindServiceHandlers are called once when a service that was already offered is stop offered.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::promise<void> service_found_barrier_after_offer_1{};
    std::promise<void> service_found_barrier_after_offer_2{};
    std::promise<void> service_found_barrier_after_stop_offer_1{};
    std::promise<void> service_found_barrier_after_stop_offer_2{};

    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_1{};
    MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> find_service_handler_2{};

    EXPECT_CALL(find_service_handler_1, Call(_, _))
        .WillOnce(InvokeWithoutArgs([&service_found_barrier_after_stop_offer_1]() {
            service_found_barrier_after_stop_offer_1.set_value();
        }));
    EXPECT_CALL(find_service_handler_2, Call(_, _))
        .WillOnce(InvokeWithoutArgs([&service_found_barrier_after_stop_offer_2]() {
            service_found_barrier_after_stop_offer_2.set_value();
        }));
    EXPECT_CALL(find_service_handler_1, Call(_, _))
        .WillOnce(InvokeWithoutArgs([&service_found_barrier_after_offer_1]() {
            service_found_barrier_after_offer_1.set_value();
        }))
        .RetiresOnSaturation();
    EXPECT_CALL(find_service_handler_2, Call(_, _))
        .WillOnce(InvokeWithoutArgs([&service_found_barrier_after_offer_2]() {
            service_found_barrier_after_offer_2.set_value();
        }))
        .RetiresOnSaturation();

    WhichContainsAServiceDiscoveryClient();

    score::cpp::ignore =
        service_discovery_client_->StartFindService(make_FindServiceHandle(1U),
                                                    CreateWrappedMockFindServiceHandler(find_service_handler_1),
                                                    kConfigStoreQm1.GetEnrichedInstanceIdentifier());

    score::cpp::ignore =
        service_discovery_client_->StartFindService(make_FindServiceHandle(2U),
                                                    CreateWrappedMockFindServiceHandler(find_service_handler_2),
                                                    kConfigStoreQm2.GetEnrichedInstanceIdentifier());

    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier());
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm2.GetInstanceIdentifier());

    service_found_barrier_after_offer_1.get_future().wait();
    service_found_barrier_after_offer_2.get_future().wait();

    score::cpp::ignore = service_discovery_client_->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(),
                                                                     IServiceDiscovery::QualityTypeSelector::kBoth);
    score::cpp::ignore = service_discovery_client_->StopOfferService(kConfigStoreQm2.GetInstanceIdentifier(),
                                                                     IServiceDiscovery::QualityTypeSelector::kBoth);

    service_found_barrier_after_stop_offer_1.get_future().wait();
    service_found_barrier_after_stop_offer_2.get_future().wait();
}

using ServiceDiscoveryClientWithFakeFileSystemStopOfferFixture = ServiceDiscoveryClientWithFakeFileSystemFixture;
TEST_F(ServiceDiscoveryClientWithFakeFileSystemStopOfferFixture, RemovesFlagFileOnStopServiceOffer)
{
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier()).has_value());
    ASSERT_TRUE(
        service_discovery_client_
            ->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth)
            .has_value());

    EXPECT_FALSE(filesystem_mock_.standard->Exists(flag_file_path_.front()).value());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemStopOfferFixture, RemovesQmFlagFileOnSelectiveStopServiceOffer)
{
    ThatSavesTheFlagFilePath().WhichContainsAServiceDiscoveryClient();

    ASSERT_TRUE(service_discovery_client_->OfferService(kConfigStoreAsilB.GetInstanceIdentifier()).has_value());
    ASSERT_TRUE(service_discovery_client_
                    ->StopOfferService(kConfigStoreAsilB.GetInstanceIdentifier(),
                                       IServiceDiscovery::QualityTypeSelector::kAsilQm)
                    .has_value());

    EXPECT_TRUE(filesystem_mock_.standard->Exists(flag_file_path_.front()).value());
    EXPECT_NE(flag_file_path_.back().Native().find(kQmPathLabel), std::string::npos);
    EXPECT_FALSE(filesystem_mock_.standard->Exists(flag_file_path_.back()).value());
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemStopOfferFixture, StopOfferingServiceThatWasNeverOfferedreturnsError)
{
    // Given a ServiceDiscoveryClient
    WhichContainsAServiceDiscoveryClient();

    // When calling StopOfferService on a service that was never offered
    const auto stop_offer_service_result = service_discovery_client_->StopOfferService(
        kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth);

    // Then an error is returned
    ASSERT_FALSE(stop_offer_service_result.has_value());
    EXPECT_EQ(stop_offer_service_result.error(), ComErrc::kBindingFailure);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemStopOfferFixture,
       StopOfferingServiceThatWasAlreadyStopOfferedreturnsError)
{
    // Given a ServiceDiscoveryClient and a service that was offered and then stop offered
    WhichContainsAServiceDiscoveryClient();
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier());
    score::cpp::ignore = service_discovery_client_->StopOfferService(kConfigStoreQm1.GetInstanceIdentifier(),
                                                                     IServiceDiscovery::QualityTypeSelector::kBoth);

    // When calling StopOfferService on the service that was already stop offered
    const auto stop_offer_service_result = service_discovery_client_->StopOfferService(
        kConfigStoreQm1.GetInstanceIdentifier(), IServiceDiscovery::QualityTypeSelector::kBoth);

    // Then an error is returned
    ASSERT_FALSE(stop_offer_service_result.has_value());
    EXPECT_EQ(stop_offer_service_result.error(), ComErrc::kBindingFailure);
}

TEST_F(ServiceDiscoveryClientWithFakeFileSystemStopOfferFixture,
       StopOfferingOfferedServiceWithInvalidQualityTypeSelectorReturnsError)
{
    // Given a ServiceDiscoveryClient and a service that was offered
    WhichContainsAServiceDiscoveryClient();
    score::cpp::ignore = service_discovery_client_->OfferService(kConfigStoreQm1.GetInstanceIdentifier());

    // When calling StopOfferService on the service with an invalid QualityTypeSelector
    const auto invalid_quality_type_selector = static_cast<IServiceDiscovery::QualityTypeSelector>(100U);
    const auto stop_offer_service_result = service_discovery_client_->StopOfferService(
        kConfigStoreQm1.GetInstanceIdentifier(), invalid_quality_type_selector);

    // Then an error is returned
    ASSERT_FALSE(stop_offer_service_result.has_value());
    EXPECT_EQ(stop_offer_service_result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl::lola::test
