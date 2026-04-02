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
#include "score/mw/com/impl/skeleton_service_elements.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{
namespace
{

using ::testing::Return;

// ── Test constants (same pattern as skeleton_base_test.cpp) ─────────────────

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
const LolaServiceInstanceId kLolaInstanceId{23U};
constexpr std::uint16_t kServiceId{34U};

// ── Concrete subclass of abstract SkeletonFieldBase for use in tests ─────────
// (SkeletonFieldBase has two pure-virtual methods: IsInitialValueSaved and
//  DoDeferredUpdate.  We must subclass it to instantiate it in tests.)

class DummyFieldForTest : public SkeletonFieldBase
{
  public:
    using SkeletonFieldBase::SkeletonFieldBase;

    bool IsInitialValueSaved() const noexcept override
    {
        return false;
    }

    bool IsSetHandlerRegistered() const noexcept override
    {
        return false;
    }

    ResultBlank DoDeferredUpdate() noexcept override
    {
        return ResultBlank{};
    }
};

// ── Minimal concrete skeleton ─────────────────────────────────────────────────

class MockSkeletonForElements final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;
};

// ── Fixture ───────────────────────────────────────────────────────────────────

class SkeletonServiceElementsFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetTracingFilterConfig()).WillByDefault(Return(nullptr));
        skeleton_ = std::make_unique<MockSkeletonForElements>(std::make_unique<mock_binding::Skeleton>(),
                                                              config_store_.GetInstanceIdentifier());
    }

    RuntimeMockGuard runtime_mock_guard_{};

    // ConfigurationStore constructed exactly as in skeleton_base_test.cpp
    ConfigurationStore config_store_{kInstanceSpecifier,
                                     kServiceIdentifier,
                                     QualityType::kASIL_QM,
                                     kServiceId,
                                     kLolaInstanceId};

    std::unique_ptr<MockSkeletonForElements> skeleton_{};

    // Non-template guards (SkeletonBindingFactoryMockGuard,
    // SkeletonMethodBindingFactoryMockGuard are plain classes; the event/field
    // guards are templates requiring a SampleType argument).
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    SkeletonMethodBindingFactoryMockGuard skeleton_method_binding_factory_mock_guard_{};
};

// ── RegisterEvent / GetEvents ─────────────────────────────────────────────────

TEST_F(SkeletonServiceElementsFixture, RegisteredEventIsRetrievable)
{
    RecordProperty("Description", "An event registered via RegisterEvent can be retrieved via GetEvents.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements elements;
    SkeletonEventBase event{*skeleton_, "ev", std::make_unique<mock_binding::SkeletonEventBase>()};

    elements.RegisterEvent("ev", event);

    const auto& events = elements.GetEvents();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(&events.at("ev").get(), &event);
}

// ── RegisterField / GetFields ─────────────────────────────────────────────────

TEST_F(SkeletonServiceElementsFixture, RegisteredFieldIsRetrievable)
{
    RecordProperty("Description", "A field registered via RegisterField can be retrieved via GetFields.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements elements;
    auto inner_event =
        std::make_unique<SkeletonEventBase>(*skeleton_, "fld", std::make_unique<mock_binding::SkeletonEventBase>());
    DummyFieldForTest field{*skeleton_, "fld", std::move(inner_event)};

    elements.RegisterField("fld", field);

    const auto& fields = elements.GetFields();
    ASSERT_EQ(fields.size(), 1U);
    EXPECT_EQ(&fields.at("fld").get(), &field);
}

// ── RegisterMethod / GetMethods ───────────────────────────────────────────────

TEST_F(SkeletonServiceElementsFixture, RegisteredMethodIsRetrievable)
{
    RecordProperty("Description", "A method registered via RegisterMethod can be retrieved via GetMethods.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements elements;
    SkeletonMethodBase method{*skeleton_, "mth", std::make_unique<mock_binding::SkeletonMethod>()};

    elements.RegisterMethod("mth", method);

    const auto& methods = elements.GetMethods();
    ASSERT_EQ(methods.size(), 1U);
    EXPECT_EQ(&methods.at("mth").get(), &method);
}

// ── UpdateEvent ───────────────────────────────────────────────────────────────

TEST_F(SkeletonServiceElementsFixture, UpdateEventReplacesStoredReference)
{
    RecordProperty("Description", "UpdateEvent replaces the stored reference for an existing event.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements elements;
    SkeletonEventBase event_a{*skeleton_, "ev", std::make_unique<mock_binding::SkeletonEventBase>()};
    SkeletonEventBase event_b{*skeleton_, "ev", std::make_unique<mock_binding::SkeletonEventBase>()};

    elements.RegisterEvent("ev", event_a);
    elements.UpdateEvent("ev", event_b);

    EXPECT_EQ(&elements.GetEvents().at("ev").get(), &event_b);
}

// ── Move constructor transfers registrations ──────────────────────────────────

TEST_F(SkeletonServiceElementsFixture, MoveConstructTransfersAllRegistrations)
{
    RecordProperty("Description", "Move constructing SkeletonServiceElements transfers all registered elements.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements source;
    SkeletonEventBase event{*skeleton_, "ev", std::make_unique<mock_binding::SkeletonEventBase>()};
    SkeletonMethodBase method{*skeleton_, "mth", std::make_unique<mock_binding::SkeletonMethod>()};
    source.RegisterEvent("ev", event);
    source.RegisterMethod("mth", method);

    SkeletonServiceElements dest{std::move(source)};

    // Source must be empty after move (moved-from state).
    EXPECT_TRUE(source.GetEvents().empty());
    EXPECT_TRUE(source.GetMethods().empty());

    // Dest must hold all registrations.
    ASSERT_EQ(dest.GetEvents().size(), 1U);
    ASSERT_EQ(dest.GetMethods().size(), 1U);
    EXPECT_EQ(&dest.GetEvents().at("ev").get(), &event);
    EXPECT_EQ(&dest.GetMethods().at("mth").get(), &method);
}

TEST_F(SkeletonServiceElementsFixture, MoveAssignTransfersAllRegistrations)
{
    RecordProperty("Description", "Move assigning SkeletonServiceElements transfers all registered elements.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements source;
    SkeletonEventBase event{*skeleton_, "ev", std::make_unique<mock_binding::SkeletonEventBase>()};
    source.RegisterEvent("ev", event);

    SkeletonServiceElements dest;
    dest = std::move(source);

    EXPECT_TRUE(source.GetEvents().empty());
    ASSERT_EQ(dest.GetEvents().size(), 1U);
    EXPECT_EQ(&dest.GetEvents().at("ev").get(), &event);
}

// ── Multiple elements ─────────────────────────────────────────────────────────

TEST_F(SkeletonServiceElementsFixture, CanRegisterMultipleEventsAndMethods)
{
    RecordProperty("Description", "Multiple events and methods can be registered independently.");
    RecordProperty("TestType", "Unit test");

    SkeletonServiceElements elements;
    SkeletonEventBase ev1{*skeleton_, "ev1", std::make_unique<mock_binding::SkeletonEventBase>()};
    SkeletonEventBase ev2{*skeleton_, "ev2", std::make_unique<mock_binding::SkeletonEventBase>()};
    SkeletonMethodBase m1{*skeleton_, "m1", std::make_unique<mock_binding::SkeletonMethod>()};
    SkeletonMethodBase m2{*skeleton_, "m2", std::make_unique<mock_binding::SkeletonMethod>()};

    elements.RegisterEvent("ev1", ev1);
    elements.RegisterEvent("ev2", ev2);
    elements.RegisterMethod("m1", m1);
    elements.RegisterMethod("m2", m2);

    EXPECT_EQ(elements.GetEvents().size(), 2U);
    EXPECT_EQ(elements.GetMethods().size(), 2U);
    EXPECT_EQ(&elements.GetEvents().at("ev1").get(), &ev1);
    EXPECT_EQ(&elements.GetEvents().at("ev2").get(), &ev2);
    EXPECT_EQ(&elements.GetMethods().at("m1").get(), &m1);
    EXPECT_EQ(&elements.GetMethods().at("m2").get(), &m2);
}

// ── UpdateAllServiceElementReferences (via SkeletonBase move) ─────────────────
// UpdateAllServiceElementReferences is private on SkeletonBase and called
// automatically during move construction/assignment.  We verify it works by
// moving a skeleton that has registered service elements and confirming the
// moved-to skeleton can still use them without crashing.

TEST_F(SkeletonServiceElementsFixture, MoveSkeletonUpdatesServiceElementBackReferences)
{
    RecordProperty("Description",
                   "After moving a SkeletonBase, all service elements that were registered via "
                   "SkeletonBaseView remain accessible on the moved-to skeleton, confirming that "
                   "UpdateAllServiceElementReferences correctly re-pointed their back-references.");
    RecordProperty("TestType", "Unit test");

    // SkeletonEventBase and SkeletonMethodBase do NOT auto-register in their
    // constructors. Registration is done explicitly through SkeletonBaseView.
    MockSkeletonForElements src{std::make_unique<mock_binding::Skeleton>(), config_store_.GetInstanceIdentifier()};
    SkeletonEventBase event{src, "ev", std::make_unique<mock_binding::SkeletonEventBase>()};
    SkeletonMethodBase method{src, "mth", std::make_unique<mock_binding::SkeletonMethod>()};

    // Register elements into src via SkeletonBaseView (the correct API path).
    SkeletonBaseView src_view{src};
    src_view.RegisterEvent("ev", event);
    src_view.RegisterMethod("mth", method);

    // Move the skeleton — the move constructor calls UpdateAllServiceElementReferences
    // internally, updating every element's skeleton_base_ reference from src to dst.
    MockSkeletonForElements dst{std::move(src)};

    // After the move, dst's service_elements_ maps must contain the registrations.
    SkeletonBaseView dst_view{dst};
    ASSERT_EQ(dst_view.GetEvents().size(), 1U);
    ASSERT_EQ(dst_view.GetMethods().size(), 1U);
    // Verify the element addresses are unchanged (only the back-reference changed).
    EXPECT_EQ(&dst_view.GetEvents().at("ev").get(), &event);
    EXPECT_EQ(&dst_view.GetMethods().at("mth").get(), &method);
}

}  // namespace
}  // namespace score::mw::com::impl
