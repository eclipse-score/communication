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
// Unit tests for functionality of GenericProxyEvent which is shared with ProxyEvent are implemented in
// proxy_event_test.cpp. Tests in this file are specific to GenericProxyEvent.
#include "score/mw/com/impl/bindings/lola/generic_proxy_event.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string_view>

namespace score::mw::com::impl::lola
{
namespace
{

class LolaGenericProxyEventFixture : public LolaProxyEventResources
{
  public:
    LolaGenericProxyEventFixture& WithAGenericProxyEvent(const ElementFqId element_fq_id, std::string_view event_name)
    {
        generic_proxy_event_ = std::make_unique<GenericProxyEvent>(*proxy_, element_fq_id, event_name);
        return *this;
    }

    // ProxyEventCommon no longer Unsubscribes on destruction, so do it explicitly.
    void TearDown() override
    {
        if (generic_proxy_event_ != nullptr)
        {
            generic_proxy_event_->Unsubscribe();
        }
        ProxyMockedMemoryFixture::TearDown();
    }

    std::unique_ptr<GenericProxyEvent> generic_proxy_event_{nullptr};
};
TEST_F(LolaGenericProxyEventFixture, CanConstructAGenericProxyEvent)
{
    // When constructing a GenericProxyEvent
    WithAGenericProxyEvent(element_fq_id_, event_name_);

    // Then a valid GenericProxyEvent is created
    EXPECT_NE(generic_proxy_event_, nullptr);
}

TEST_F(LolaGenericProxyEventFixture, GetSampleSize)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description",
                   "Checks that GetSampleSize will return the sample size of the underlying event data type.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid GenericProxyEvent
    WithAGenericProxyEvent(element_fq_id_, event_name_);

    // Expect, that asking about the Sample size, we get the sizeof the underlying event data type (which is
    // std::uint32_t in case of LolaProxyEventResources)
    EXPECT_EQ(generic_proxy_event_->GetSampleSize(), sizeof(SampleType));
}

TEST_F(LolaGenericProxyEventFixture, HasSerializedFormat)
{
    RecordProperty("Verifies", "SCR-14035199");
    RecordProperty("Description", "Checks that HasSerializedFormat will always return false for the Lola binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid GenericProxyEvent
    WithAGenericProxyEvent(element_fq_id_, event_name_);

    // Expect, that asking about the serialized format, we get "FALSE"
    EXPECT_EQ(generic_proxy_event_->HasSerializedFormat(), false);
}

TEST_F(LolaGenericProxyEventFixture, SampleConstness)
{
    RecordProperty("Verifies", "SCR-6340729");
    RecordProperty("Description", "Proxy shall interpret slot data as const");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid GenericProxyEvent
    WithAGenericProxyEvent(element_fq_id_, event_name_);

    // Then the MetaInfoMemberType should hold const slot data
    GenericProxyEventAttorney generic_proxy_event_attorney{*generic_proxy_event_};
    using MetaInfoMemberType =
        typename std::remove_reference<decltype(generic_proxy_event_attorney.GetMetaInfoMember())>::type;
    static_assert(std::is_const<MetaInfoMemberType>::value, "Proxy should hold const slot data.");
}

using LolaGenericProxyEventDeathTest = LolaGenericProxyEventFixture;
TEST_F(LolaGenericProxyEventDeathTest, FailOnEventNotFound)
{
    const ElementFqId bad_element_fq_id{0xcdef, 0x6, 0x10, ServiceElementType::EVENT};
    const std::string_view bad_event_name{"BadEventName"};

    // When constructing a GenericProxyEvent from an unknown event
    // Then the program terminates
    EXPECT_DEATH(WithAGenericProxyEvent(bad_element_fq_id, bad_event_name), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
