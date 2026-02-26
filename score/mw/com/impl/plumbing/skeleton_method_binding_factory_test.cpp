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

#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory.h"

#include "score/mw/com/impl/bindings/lola/test/skeleton_test_resources.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <memory>

namespace score::mw::com::impl
{
using TestSampleType = std::uint32_t;

constexpr auto kDummyMethodName{"Method8"};

constexpr std::uint16_t kDummyMethodId{6U};

constexpr uint16_t kInstanceId = 0x31U;
const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value();

const auto kQueueSize{23U};
const LolaServiceInstanceDeployment kLolaServiceInstanceDeployment{
    LolaServiceInstanceId{kInstanceId},
    {},
    {},
    {{kDummyMethodName, LolaMethodInstanceDeployment{kQueueSize}}}};

const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId, {}, {}, {{kDummyMethodName, kDummyMethodId}}};

ConfigurationStore kConfigStoreAsilB{kInstanceSpecifier,
                                     make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
                                     QualityType::kASIL_B,
                                     kLolaServiceTypeDeployment,
                                     kLolaServiceInstanceDeployment};

class SkeletonMethodFactoryFixture : public lola::SkeletonMockedMemoryFixture
{
  public:
    InstanceIdentifier GetValidLoLaInstanceIdentifier()
    {
        return kConfigStoreAsilB.GetInstanceIdentifier();
    }

    InstanceIdentifier GetValidSomeIpInstanceIdentifier()
    {
        return dummy_instance_identifier_builder_.CreateSomeIpBindingInstanceIdentifier();
    }

    InstanceIdentifier GetBlankBindingInstanceIdentifier()
    {
        return dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
    }

    SkeletonBinding* GetBindingFromInstanceIdentifier(const InstanceIdentifier& instance_identifier)
    {
        skeleton_base_ = std::make_unique<SkeletonBase>(std::move(this->skeleton_), instance_identifier);
        return SkeletonBaseView{*skeleton_base_}.GetBinding();
    }

    std::unique_ptr<SkeletonBase> skeleton_base_{nullptr};

  private:
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(SkeletonMethodFactoryFixture, CanConstructSkeletonMethod)
{
    const auto instance_identifier = GetValidLoLaInstanceIdentifier();

    // Given a SkeletonBase with a valid lola binding
    InitialiseSkeleton(instance_identifier);

    // When creating a SkeletonMethod using MethodBindingFactory
    auto skeleton_method = SkeletonMethodBindingFactory::Create(instance_identifier, skeleton_.get(), kDummyMethodName);

    // Then a valid binding can be created
    ASSERT_NE(skeleton_method, nullptr);
}

TEST_F(SkeletonMethodFactoryFixture, CannotCreateSkeletonServiceWhenSkeletonBindingIsNullptr)
{
    const auto instance_identifier = GetValidLoLaInstanceIdentifier();

    // Given a SkeletonBase that does not contain a valid binding i.e. the binding is a nullptr
    auto skeleton_base{nullptr};

    // When creating a SkeletonMethod using MethodBindingFactory
    auto skeleton_method = SkeletonMethodBindingFactory::Create(instance_identifier, skeleton_base, kDummyMethodName);

    // Then a nullptr is returned
    ASSERT_EQ(skeleton_method, nullptr);
}

TEST_F(SkeletonMethodFactoryFixture, CannotConstructEventFromSomeIpBinding)
{
    const auto instance_identifier = GetValidSomeIpInstanceIdentifier();

    // Given a valid SomeIp binding
    auto skeleton_binding = GetBindingFromInstanceIdentifier(instance_identifier);

    // When creating a SkeletonMethod using MethodBindingFactory
    auto skeleton_method =
        SkeletonMethodBindingFactory::Create(instance_identifier, skeleton_binding, kDummyMethodName);

    // Then a nullptr is returned
    EXPECT_EQ(skeleton_method, nullptr);
}

TEST_F(SkeletonMethodFactoryFixture, CannotConstructEventFromBlankBinding)
{
    const auto instance_identifier = GetBlankBindingInstanceIdentifier();

    // Given a blank binding
    auto skeleton_binding = GetBindingFromInstanceIdentifier(instance_identifier);

    // When creating a SkeletonMethod using MethodBindingFactory
    auto skeleton_method =
        SkeletonMethodBindingFactory::Create(instance_identifier, skeleton_binding, kDummyMethodName);

    // Then a nullptr is returned
    EXPECT_EQ(skeleton_method, nullptr);
}

}  // namespace score::mw::com::impl
