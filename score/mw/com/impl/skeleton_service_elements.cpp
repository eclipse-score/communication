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

// Full type definitions are needed here; skeleton_base.h is NOT included because
// skeleton_service_elements is a dependency OF skeleton_base — including skeleton_base.h
// here would create a circular dependency.  The UpdateSkeletonReference loops that require
// a complete SkeletonBase type live in skeleton_base.cpp::UpdateAllServiceElementReferences().
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/skeleton_field_base.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

namespace score::mw::com::impl
{

void SkeletonServiceElements::RegisterEvent(const std::string_view event_name, SkeletonEventBase& event) noexcept
{
    const auto result = events_.emplace(event_name, event);
    const bool was_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_inserted, "Event cannot be registered as it already exists.");
}

void SkeletonServiceElements::RegisterField(const std::string_view field_name, SkeletonFieldBase& field) noexcept
{
    const auto result = fields_.emplace(field_name, field);
    const bool was_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_inserted, "Field cannot be registered as it already exists.");
}

void SkeletonServiceElements::RegisterMethod(const std::string_view method_name, SkeletonMethodBase& method) noexcept
{
    const auto result = methods_.emplace(method_name, method);
    const bool was_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_inserted, "Method cannot be registered as it already exists.");
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from std::map::at in case the container doesn't
// have the mapped element. This function is called by the move constructor of generated Skeleton classes and we
// register the event name in the events_ container during SkeletonEvent construction, so we are sure that the
// event_name already exists — no way for throwing std::out_of_range which leads to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void SkeletonServiceElements::UpdateEvent(const std::string_view event_name, SkeletonEventBase& event) noexcept
{
    // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "If a function is declared to be
    // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception".
    // The event_name is guaranteed to exist (registered during SkeletonEvent construction).
    // coverity[autosar_cpp14_a15_4_2_violation : FALSE]
    events_.at(event_name) = event;
}

void SkeletonServiceElements::UpdateField(const std::string_view field_name, SkeletonFieldBase& field) noexcept
{
    auto it = fields_.find(field_name);
    if (it == fields_.cend())
    {
        score::mw::log::LogError("lola") << "SkeletonServiceElements::UpdateField: field '" << field_name
                                         << "' not found.";
        std::terminate();
    }
    it->second = field;
}

void SkeletonServiceElements::UpdateMethod(const std::string_view method_name, SkeletonMethodBase& method) noexcept
{
    auto it = methods_.find(method_name);
    if (it == methods_.cend())
    {
        score::mw::log::LogError("lola") << "SkeletonServiceElements::UpdateMethod: method '" << method_name
                                         << "' not found.";
        std::terminate();
    }
    it->second = method;
}

}  // namespace score::mw::com::impl
