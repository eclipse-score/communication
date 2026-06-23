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
#include "score/mw/com/impl/bindings/lola/proxy_element_lookup.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/configuration/lola_service_element_id.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_type.h"

#include <score/blank.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace score::mw::com::impl::lola
{
namespace
{

const LolaServiceId kServiceId{0x1234U};
const LolaServiceElementId kEventId{0x10U};
const LolaServiceElementId kFieldId{0x20U};
const LolaServiceElementId kMethodId{0x30U};
const LolaServiceInstanceId::InstanceId kInstanceId{0x42U};

const std::string kEventName{"my_event"};
const std::string kFieldName{"my_field"};
const std::string kMethodName{"my_method"};
const std::string kUnknownName{"name_that_does_not_exist"};

/// \brief Fixture providing a HandleType backed by a lola deployment that contains one named event, field and method.
class GetElementFqIdFixture : public ::testing::Test
{
  protected:
    // The ConfigurationStore owns the deployments the HandleType points to, so it must outlive handle_.
    ConfigurationStore config_store_{
        InstanceSpecifier::Create(std::string{"/proxy_element_lookup_test_instance"}).value(),
        make_ServiceIdentifierType("proxy_element_lookup_test_service"),
        QualityType::kASIL_QM,
        LolaServiceTypeDeployment{kServiceId,
                                  {{kEventName, kEventId}},
                                  {{kFieldName, kFieldId}},
                                  {{kMethodName, kMethodId}}},
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}}};
    HandleType handle_{config_store_.GetHandle()};
};

TEST_F(GetElementFqIdFixture, ResolvesEventElement)
{
    // Given a handle whose lola deployment contains the event "my_event"
    const ElementFqId expected_fq_id{kServiceId, kEventId, kInstanceId, ServiceElementType::EVENT};

    // When resolving that event name as an EVENT
    const auto result = GetElementFqId(handle_, kEventName, ServiceElementType::EVENT);

    // Then the fully qualified id for the event is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected_fq_id);
    // ElementFqId::operator== intentionally ignores element_type_, so assert the resolved type explicitly.
    EXPECT_EQ(result.value().element_type_, ServiceElementType::EVENT);
}

TEST_F(GetElementFqIdFixture, ResolvesFieldElement)
{
    // Given a handle whose lola deployment contains the field "my_field"
    const ElementFqId expected_fq_id{kServiceId, kFieldId, kInstanceId, ServiceElementType::FIELD};

    // When resolving that field name as a FIELD
    const auto result = GetElementFqId(handle_, kFieldName, ServiceElementType::FIELD);

    // Then the fully qualified id for the field is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected_fq_id);
    // ElementFqId::operator== intentionally ignores element_type_, so assert the resolved type explicitly.
    EXPECT_EQ(result.value().element_type_, ServiceElementType::FIELD);
}

TEST_F(GetElementFqIdFixture, ResolvesMethodElement)
{
    // Given a handle whose lola deployment contains the method "my_method"
    const ElementFqId expected_fq_id{kServiceId, kMethodId, kInstanceId, ServiceElementType::METHOD};

    // When resolving that method name as a METHOD
    const auto result = GetElementFqId(handle_, kMethodName, ServiceElementType::METHOD);

    // Then the fully qualified id for the method is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected_fq_id);
    // ElementFqId::operator== intentionally ignores element_type_, so assert the resolved type explicitly.
    EXPECT_EQ(result.value().element_type_, ServiceElementType::METHOD);
}

TEST_F(GetElementFqIdFixture, ReturnsNulloptForNonLolaDeployment)
{
    // Given a handle whose service type deployment is blank (non-lola), while still carrying a lola instance id
    const ServiceTypeDeployment blank_type_deployment{score::cpp::blank{}};
    const ServiceInstanceDeployment instance_deployment{
        make_ServiceIdentifierType("proxy_element_lookup_test_service"),
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
        QualityType::kASIL_QM,
        InstanceSpecifier::Create(std::string{"/proxy_element_lookup_test_blank_instance"}).value()};
    const auto identifier = make_InstanceIdentifier(instance_deployment, blank_type_deployment);
    const auto blank_handle = make_HandleType(identifier);

    // When resolving any element name
    const auto result = GetElementFqId(blank_handle, kEventName, ServiceElementType::EVENT);

    // Then no fully qualified id is returned
    EXPECT_EQ(result, std::nullopt);
}

using GetElementFqIdDeathTest = GetElementFqIdFixture;

TEST_F(GetElementFqIdDeathTest, TerminatesWhenElementNameNotInDeployment)
{
    // Given a handle whose lola deployment does not contain the requested element name
    // When resolving that unknown name
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqId(handle_, kUnknownName, ServiceElementType::EVENT), ".*");
}

TEST_F(GetElementFqIdDeathTest, TerminatesForInvalidElementType)
{
    // Given a handle with a valid lola deployment
    // When resolving an element with an invalid element type
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqId(handle_, kEventName, ServiceElementType::INVALID), ".*");
}

TEST_F(GetElementFqIdDeathTest, TerminatesForNonLolaInstanceId)
{
    // Given a handle whose lola type deployment is valid but whose instance id is non-lola (blank)
    const auto non_lola_instance_handle = config_store_.GetHandle(ServiceInstanceId{score::cpp::blank{}});

    // When resolving an element name (the lola type deployment selects the lola lookup path)
    // Then the mismatched non-lola instance id terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqId(non_lola_instance_handle, kEventName, ServiceElementType::EVENT),
                 ".*");
}

TEST(GetLolaProxyBindingTest, ReturnsNullptrForNullBinding)
{
    // Given a null parent binding
    ProxyBinding* const parent_binding{nullptr};

    // When asking for the lola proxy binding
    // Then nullptr is returned
    EXPECT_EQ(GetLolaProxyBinding(parent_binding), nullptr);
}

TEST(GetLolaProxyBindingTest, ReturnsNullptrForNonLolaBinding)
{
    // Given a non-lola proxy binding
    mock_binding::Proxy non_lola_binding{};
    ProxyBinding* const parent_binding{&non_lola_binding};

    // When asking for the lola proxy binding
    // Then nullptr is returned
    EXPECT_EQ(GetLolaProxyBinding(parent_binding), nullptr);
}

/// \brief Fixture providing a fully constructed lola::Proxy binding.
class GetLolaProxyBindingFixture : public ProxyMockedMemoryFixture
{
};

TEST_F(GetLolaProxyBindingFixture, ReturnsLolaProxyForLolaBinding)
{
    // Given a real lola proxy binding
    InitialiseProxyWithConstructor(identifier_);
    ProxyBinding* const parent_binding{proxy_.get()};

    // When asking for the lola proxy binding
    // Then the same lola proxy is returned
    EXPECT_EQ(GetLolaProxyBinding(parent_binding), proxy_.get());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
