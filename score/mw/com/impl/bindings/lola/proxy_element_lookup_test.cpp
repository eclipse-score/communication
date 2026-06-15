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
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/configuration/lola_service_element_id.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_binding.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace score::mw::com::impl::lola
{
namespace
{

const LolaServiceId kServiceId{0xABCDU};
constexpr std::uint16_t kInstanceId{0x42U};

const std::string kEventName{"Event1"};
const std::string kFieldName{"Field1"};
const std::string kMethodName{"Method1"};
constexpr LolaServiceElementId kEventId{10U};
constexpr LolaServiceElementId kFieldId{20U};
constexpr LolaServiceElementId kMethodId{30U};

const LolaServiceTypeDeployment kLolaServiceTypeDeployment{kServiceId,
                                                           {{kEventName, kEventId}},
                                                           {{kFieldName, kFieldId}},
                                                           {{kMethodName, kMethodId}}};

class ProxyElementLookupFixture : public ProxyMockedMemoryFixture
{
  public:
    HandleType GetLolaHandle()
    {
        return config_store_.GetHandle();
    }

    HandleType GetBlankBindingHandle()
    {
        const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
        return make_HandleType(instance_identifier, ServiceInstanceId{LolaServiceInstanceId{kInstanceId}});
    }

    ConfigurationStore config_store_{
        InstanceSpecifier::Create(std::string{"/proxy_element_lookup_instance_specifier"}).value(),
        make_ServiceIdentifierType("/a/service/somewhere/out/there", 13U, 37U),
        QualityType::kASIL_B,
        kLolaServiceTypeDeployment,
        LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}}};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

using ProxyElementLookupDeathTest = ProxyElementLookupFixture;

TEST_F(ProxyElementLookupFixture, GetLolaProxyBindingReturnsNullptrForNullBinding)
{
    // Given a null parent binding
    ProxyBinding* const parent_binding = nullptr;

    // When resolving the lola proxy binding
    // Then a nullptr is returned
    EXPECT_EQ(GetLolaProxyBinding(parent_binding), nullptr);
}

TEST_F(ProxyElementLookupFixture, GetLolaProxyBindingReturnsNullptrForNonLolaBinding)
{
    // Given a parent binding that is not a lola binding
    mock_binding::Proxy non_lola_binding{};
    ProxyBinding* const parent_binding = &non_lola_binding;

    // When resolving the lola proxy binding
    // Then a nullptr is returned because the dynamic_cast to lola::Proxy fails
    EXPECT_EQ(GetLolaProxyBinding(parent_binding), nullptr);
}

TEST_F(ProxyElementLookupFixture, GetLolaProxyBindingReturnsProxyForLolaBinding)
{
    // Given a real lola proxy binding
    InitialiseProxyWithConstructor(identifier_);
    ProxyBinding* const parent_binding = proxy_.get();

    // When resolving the lola proxy binding
    // Then the same lola proxy is returned
    EXPECT_EQ(GetLolaProxyBinding(parent_binding), proxy_.get());
}

TEST_F(ProxyElementLookupFixture, GetElementFqIdResolvesEvent)
{
    // Given a handle to a lola deployment containing an event
    const auto handle = GetLolaHandle();

    // When resolving the event by name
    const auto element_fq_id = GetElementFqId(handle, kEventName, ServiceElementType::EVENT);

    // Then the fully qualified id for the event is returned (operator== ignores element_type_, so check it explicitly)
    ASSERT_TRUE(element_fq_id.has_value());
    EXPECT_EQ(*element_fq_id, ElementFqId(kServiceId, kEventId, kInstanceId, ServiceElementType::EVENT));
    EXPECT_EQ(element_fq_id->element_type_, ServiceElementType::EVENT);
}

TEST_F(ProxyElementLookupFixture, GetElementFqIdResolvesField)
{
    // Given a handle to a lola deployment containing a field
    const auto handle = GetLolaHandle();

    // When resolving the field by name
    const auto element_fq_id = GetElementFqId(handle, kFieldName, ServiceElementType::FIELD);

    // Then the fully qualified id for the field is returned
    ASSERT_TRUE(element_fq_id.has_value());
    EXPECT_EQ(*element_fq_id, ElementFqId(kServiceId, kFieldId, kInstanceId, ServiceElementType::FIELD));
    EXPECT_EQ(element_fq_id->element_type_, ServiceElementType::FIELD);
}

TEST_F(ProxyElementLookupFixture, GetElementFqIdResolvesMethod)
{
    // Given a handle to a lola deployment containing a method
    const auto handle = GetLolaHandle();

    // When resolving the method by name
    const auto element_fq_id = GetElementFqId(handle, kMethodName, ServiceElementType::METHOD);

    // Then the fully qualified id for the method is returned
    ASSERT_TRUE(element_fq_id.has_value());
    EXPECT_EQ(*element_fq_id, ElementFqId(kServiceId, kMethodId, kInstanceId, ServiceElementType::METHOD));
    EXPECT_EQ(element_fq_id->element_type_, ServiceElementType::METHOD);
}

TEST_F(ProxyElementLookupFixture, GetElementFqIdReturnsNulloptForBlankDeployment)
{
    // Given a handle whose deployment is not a lola (blank) deployment
    const auto handle = GetBlankBindingHandle();

    // When resolving any element
    const auto element_fq_id = GetElementFqId(handle, kEventName, ServiceElementType::EVENT);

    // Then no element id is returned
    EXPECT_FALSE(element_fq_id.has_value());
}

// Note: GetElementFqId is noexcept, so a contract violation in the lookup escapes as std::terminate rather than the
// throwing handler used by SCORE_LANGUAGE_FUTURECPP_*_CONTRACT_VIOLATED. We therefore use EXPECT_DEATH here.
TEST_F(ProxyElementLookupDeathTest, GetElementFqIdTerminatesForUnknownElementName)
{
    // Given a handle to a lola deployment
    const auto handle = GetLolaHandle();

    // When resolving an element name that is absent from the deployment
    // Then the lookup terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqId(handle, "ThisElementDoesNotExist", ServiceElementType::EVENT),
                 ".*");
}

TEST_F(ProxyElementLookupDeathTest, GetElementFqIdTerminatesForInvalidElementType)
{
    // Given a handle to a lola deployment
    const auto handle = GetLolaHandle();

    // When resolving with an invalid service element type
    // Then the lookup terminates
    EXPECT_DEATH(score::cpp::ignore = GetElementFqId(handle, kEventName, ServiceElementType::INVALID), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
