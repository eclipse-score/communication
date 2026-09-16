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
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory.h"

namespace score::mw::com::impl
{

auto SkeletonFieldBindingFactory::instance() noexcept -> ISkeletonFieldBindingFactory&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-2-2", The rule states: "Static and thread-local objects shall be constant-initialized"
    // It cannot be made const since we will need to call non-const methods from a static instance.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static SkeletonFieldBindingFactoryImpl instance{};
    return instance;
}

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file in multiple
// translation units without violating the One Definition Rule."
// The static member mock_ in the SkeletonFieldBindingFactory is intentionally defined in the header file
// to facilitate template instantiation across multiple translation units used in diff applications.
ISkeletonFieldBindingFactory* SkeletonFieldBindingFactory::mock_{nullptr};
}  // namespace score::mw::com::impl
