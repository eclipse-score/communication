/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

/// \file
/// \brief End-to-end test of the SOME/IP binding dispatch through the binding independent plumbing.
///
/// The tests set the SOME/IP binding purely through the deployment configuration (there is no other switch) and then
/// assert that the binding independent factories create SOME/IP binding objects which carry exactly the values that
/// were configured. This is the observable proof that the binding selection works and that the configured values reach
/// the binding.

#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/someip/proxy.h"
#include "score/mw/com/impl/bindings/someip/proxy_event.h"
#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/plumbing/binding_factory_error.h"
#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/service_element_type.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using SampleType = std::uint32_t;

constexpr SomeIpServiceId kServiceId{0x1234U};
constexpr SomeIpServiceInstanceId::InstanceId kInstanceId{0x0007U};
constexpr SomeIpEventId kTemperatureEventId{0x8001U};
constexpr SomeIpEventId kPressureEventId{0x8002U};
constexpr auto kTemperatureEventName{"temperature"};
constexpr auto kUnknownEventName{"not_configured"};

/// \brief Builds an InstanceIdentifier whose deployment selects the SOME/IP binding.
///
/// This is the only place where the binding is chosen: putting SomeIpServiceInstanceDeployment and
/// SomeIpServiceTypeDeployment into the deployment variants is what makes the whole plumbing take the SOME/IP path.
class SomeIpDeploymentBuilder
{
  public:
    SomeIpDeploymentBuilder()
        : instance_binding_{SomeIpServiceInstanceId{kInstanceId}},
          type_binding_{kServiceId, {{kTemperatureEventName, kTemperatureEventId}, {"pressure", kPressureEventId}}},
          type_deployment_{type_binding_},
          instance_deployment_{make_ServiceIdentifierType("SomeIpTestService"),
                               instance_binding_,
                               QualityType::kASIL_QM,
                               InstanceSpecifier::Create("/someip_test_instance").value()}
    {
    }

    InstanceIdentifier GetInstanceIdentifier() const
    {
        return make_InstanceIdentifier(instance_deployment_, type_deployment_);
    }

    HandleType GetHandle() const
    {
        return make_HandleType(GetInstanceIdentifier(), ServiceInstanceId{SomeIpServiceInstanceId{kInstanceId}});
    }

    const ServiceInstanceDeployment& GetInstanceDeployment() const noexcept
    {
        return instance_deployment_;
    }

  private:
    SomeIpServiceInstanceDeployment instance_binding_;
    SomeIpServiceTypeDeployment type_binding_;
    ServiceTypeDeployment type_deployment_;
    ServiceInstanceDeployment instance_deployment_;
};

class SomeIpBindingDispatchFixture : public ::testing::Test
{
  protected:
    SomeIpDeploymentBuilder deployment_{};
};

TEST_F(SomeIpBindingDispatchFixture, DeploymentWithSomeIpBindingReportsSomeIpBindingType)
{
    // Given a service instance deployment that was configured with the SOME/IP binding
    // When asking the binding independent deployment for its binding type
    const auto binding_type = deployment_.GetInstanceDeployment().GetBindingType();

    // Then it reports the SOME/IP binding
    EXPECT_EQ(binding_type, BindingType::kSomeIp);
}

TEST_F(SomeIpBindingDispatchFixture, ProxyBindingFactoryCreatesSomeIpProxyCarryingTheConfiguredServiceInstance)
{
    // Given a handle whose deployment selects the SOME/IP binding
    const auto handle = deployment_.GetHandle();

    // When creating the proxy binding through the binding independent factory
    const auto proxy_binding_result = ProxyBindingFactory::Create(handle);
    ASSERT_TRUE(proxy_binding_result.has_value());

    // Then a SOME/IP proxy is created
    auto* const someip_proxy = dynamic_cast<someip::Proxy*>(proxy_binding_result.value().get());
    ASSERT_NE(someip_proxy, nullptr);

    // ... and it is not a proxy of any other binding
    EXPECT_EQ(dynamic_cast<lola::Proxy*>(proxy_binding_result.value().get()), nullptr);

    // ... and it carries exactly the service instance that was configured
    EXPECT_EQ(someip_proxy->GetEndpoint().GetServiceId(), kServiceId);
    EXPECT_EQ(someip_proxy->GetEndpoint().GetInstanceId(), kInstanceId);

    // ... and it knows which events the configured service interface provides
    EXPECT_TRUE(someip_proxy->IsEventProvided(kTemperatureEventName));
    EXPECT_FALSE(someip_proxy->IsEventProvided(kUnknownEventName));

    proxy_binding_result.value()->PrepareDeinitialize();
    proxy_binding_result.value()->FinalizeDeinitialize();
}

TEST_F(SomeIpBindingDispatchFixture, SkeletonBindingFactoryCreatesSomeIpSkeletonCarryingTheConfiguredServiceInstance)
{
    // Given an instance identifier whose deployment selects the SOME/IP binding
    const auto identifier = deployment_.GetInstanceIdentifier();

    // When creating the skeleton binding through the binding independent factory
    const auto skeleton_binding = SkeletonBindingFactory::Create(identifier);
    ASSERT_NE(skeleton_binding, nullptr);

    // Then the created binding reports the SOME/IP binding type
    EXPECT_EQ(skeleton_binding->GetBindingType(), BindingType::kSomeIp);

    // ... and it is a SOME/IP skeleton carrying the configured service instance
    auto* const someip_skeleton = dynamic_cast<someip::Skeleton*>(skeleton_binding.get());
    ASSERT_NE(someip_skeleton, nullptr);
    EXPECT_EQ(someip_skeleton->GetEndpoint().GetServiceId(), kServiceId);
    EXPECT_EQ(someip_skeleton->GetEndpoint().GetInstanceId(), kInstanceId);
}

TEST_F(SomeIpBindingDispatchFixture, ProxyEventBindingFactoryCreatesSomeIpProxyEventWithConfiguredEventId)
{
    // Given a SOME/IP proxy binding created from the deployment
    const auto handle = deployment_.GetHandle();
    const auto proxy_binding_result = ProxyBindingFactory::Create(handle);
    ASSERT_TRUE(proxy_binding_result.has_value());
    auto& proxy_binding = *proxy_binding_result.value();

    // When creating the proxy event binding for a configured event
    const auto proxy_event_result = ProxyEventBindingFactory<SampleType>::Create(
        handle, proxy_binding, kTemperatureEventName, ServiceElementType::EVENT);
    ASSERT_TRUE(proxy_event_result.has_value());

    // Then the created event binding reports the SOME/IP binding type
    EXPECT_EQ(proxy_event_result.value()->GetBindingType(), BindingType::kSomeIp);

    // ... and it resolved the wire event id from the SOME/IP service type deployment
    auto* const someip_proxy_event = dynamic_cast<someip::ProxyEvent<SampleType>*>(proxy_event_result.value().get());
    ASSERT_NE(someip_proxy_event, nullptr);
    EXPECT_EQ(someip_proxy_event->GetEventId(), kTemperatureEventId);
    EXPECT_EQ(someip_proxy_event->GetEventName(), kTemperatureEventName);

    // ... and it is attached to the SOME/IP proxy that the plumbing created before
    EXPECT_EQ(&someip_proxy_event->GetParent(), dynamic_cast<someip::Proxy*>(&proxy_binding));

    proxy_binding.PrepareDeinitialize();
    proxy_binding.FinalizeDeinitialize();
}

TEST_F(SomeIpBindingDispatchFixture, SkeletonEventBindingFactoryCreatesSomeIpSkeletonEventWithConfiguredEventId)
{
    // Given a SOME/IP skeleton binding created from the deployment
    const auto identifier = deployment_.GetInstanceIdentifier();
    const auto skeleton_binding = SkeletonBindingFactory::Create(identifier);
    ASSERT_NE(skeleton_binding, nullptr);

    // When creating the skeleton event binding for a configured event
    const auto skeleton_event =
        SkeletonEventBindingFactory<SampleType>::Create(identifier, *skeleton_binding, kTemperatureEventName);
    ASSERT_NE(skeleton_event, nullptr);

    // Then the created event binding reports the SOME/IP binding type
    EXPECT_EQ(skeleton_event->GetBindingType(), BindingType::kSomeIp);

    // ... and it resolved the wire event id from the SOME/IP service type deployment
    auto* const someip_skeleton_event = dynamic_cast<someip::SkeletonEvent<SampleType>*>(skeleton_event.get());
    ASSERT_NE(someip_skeleton_event, nullptr);
    EXPECT_EQ(someip_skeleton_event->GetEventId(), kTemperatureEventId);
    EXPECT_EQ(someip_skeleton_event->GetEventName(), kTemperatureEventName);

    // ... and it is attached to the SOME/IP skeleton that the plumbing created before
    EXPECT_EQ(&someip_skeleton_event->GetParent(), dynamic_cast<someip::Skeleton*>(skeleton_binding.get()));
}

TEST_F(SomeIpBindingDispatchFixture, ProxyEventBindingFactoryRejectsParentBindingOfADifferentBinding)
{
    // Given a parent proxy binding that does not belong to the SOME/IP binding
    class NonSomeIpProxyBinding final : public ProxyBinding
    {
      public:
        bool IsEventProvided(const std::string_view) const override
        {
            return true;
        }
        Result<void> SetupMethods(const std::size_t) override
        {
            return {};
        }
        void PrepareDeinitialize() override {}
        void FinalizeDeinitialize() override {}
    };
    NonSomeIpProxyBinding foreign_parent_binding{};

    // When creating a proxy event binding from a SOME/IP deployment with that parent
    const auto proxy_event_result = ProxyEventBindingFactory<SampleType>::Create(
        deployment_.GetHandle(), foreign_parent_binding, kTemperatureEventName, ServiceElementType::EVENT);

    // Then creation fails because parent and service element would belong to different bindings
    ASSERT_FALSE(proxy_event_result.has_value());
    EXPECT_EQ(proxy_event_result.error(), BindingFactoryErrorCode::kParentBindingTypeMismatch);
}

TEST_F(SomeIpBindingDispatchFixture, InstanceIdentifierExposesSomeIpInstanceId)
{
    // Given an instance identifier whose deployment selects the SOME/IP binding
    const auto identifier = deployment_.GetInstanceIdentifier();

    // When asking the binding independent view for the service instance id
    const auto service_instance_id = InstanceIdentifierView{identifier}.GetServiceInstanceId();
    ASSERT_TRUE(service_instance_id.has_value());

    // Then it holds the SOME/IP instance id from the deployment
    const auto* const someip_instance_id =
        std::get_if<SomeIpServiceInstanceId>(&service_instance_id.value().binding_info_);
    ASSERT_NE(someip_instance_id, nullptr);
    EXPECT_EQ(someip_instance_id->GetId(), kInstanceId);
}

}  // namespace
}  // namespace score::mw::com::impl
