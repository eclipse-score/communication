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
#include "score/mw/com/runtime.h"
#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/runtime.h"

#include <score/span.hpp>

#include <cstdint>

score::Result<score::mw::com::InstanceIdentifierContainer> score::mw::com::runtime::ResolveInstanceIDs(
    const impl::InstanceSpecifier model_name)
{
    const auto instance_identifier_container = score::mw::com::impl::Runtime::getInstance().resolve(model_name);
    if (instance_identifier_container.empty())
    {
        return MakeUnexpected(impl::ComErrc::kInstanceIDCouldNotBeResolved,
                              "Binding returned empty vector of instance identifiers");
    }
    return instance_identifier_container;
}

// NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
void score::mw::com::runtime::InitializeRuntime(const std::int32_t argc, score::StringLiteral argv[])
{
    const score::cpp::span<const score::StringLiteral> args(argv, argc);
    score::mw::com::impl::Runtime::Initialize(args);
}
