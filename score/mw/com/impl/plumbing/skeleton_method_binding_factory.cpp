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
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory_impl.h"

namespace score::mw::com::impl
{

std::unique_ptr<SkeletonMethodBinding> SkeletonMethodBindingFactory::Create(
    const InstanceIdentifier& instance_identifier,
    SkeletonBinding* parent_binding,
    const std::string_view method_name)
{
    return instance().Create(instance_identifier, parent_binding, method_name);
}

auto SkeletonMethodBindingFactory::instance() -> ISkeletonMethodBindingFactory&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }
    // Suppress "AUTOSAR C++14 A3-3-2", The rule states: "Static and thread-local objects shall be
    // constant-initialized" It cannot be made const since we will need to call non-const methods from a static
    // instance. coverity[autosar_cpp14_a3_3_2_violation]
    static SkeletonMethodBindingFactoryImpl instance{};
    return instance;
}

// Suppress "AUTOSAR_C++14_A3-1-1" rule finding. This rule states:" It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule".
// coverity[autosar_cpp14_a3_1_1_violation] This is false-positive, "mock_" is defined once
ISkeletonMethodBindingFactory* SkeletonMethodBindingFactory::mock_ = nullptr;
}  // namespace score::mw::com::impl
