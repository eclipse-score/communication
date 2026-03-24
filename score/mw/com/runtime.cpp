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

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/runtime.h"

#include "score/string/string_literal.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <cstdint>
#include <utility>

namespace score::mw::com::runtime
{
namespace detail
{

IRuntime* RuntimeMockHolder::runtime_mock_{nullptr};

}

score::Result<InstanceIdentifierContainer> ResolveInstanceIDs(const impl::InstanceSpecifier model_name)
{
    if (auto* const runtime_mock_holder = detail::RuntimeMockHolder::GetRuntimeMock())
    {
        return runtime_mock_holder->ResolveInstanceIDs(model_name);
    }

    const auto instance_identifier_container = impl::Runtime::getInstance().resolve(model_name);
    if (instance_identifier_container.empty())
    {
        return MakeUnexpected(impl::ComErrc::kInstanceIDCouldNotBeResolved,
                              "Binding returned empty vector of instance identifiers");
    }
    return instance_identifier_container;
}

// NOLINTNEXTLINE(modernize-avoid-c-arrays):C-style array tolerated for command line arguments
void InitializeRuntime(const std::int32_t argc, score::StringLiteral argv[])
{
    if (auto* const runtime_mock_holder = detail::RuntimeMockHolder::GetRuntimeMock())
    {
        const score::cpp::span<const score::StringLiteral> argv_span(
            argv, static_cast<score::cpp::span<const score::StringLiteral>::size_type>(argc));
        runtime_mock_holder->InitializeRuntime(argc, argv_span);
        return;
    }

    const RuntimeConfiguration runtime_configuration{argc, argv};
    InitializeRuntime(runtime_configuration);
}

void InitializeRuntime(const RuntimeConfiguration& runtime_configuration)
{
    if (auto* const runtime_mock_holder = detail::RuntimeMockHolder::GetRuntimeMock())
    {
        runtime_mock_holder->InitializeRuntime(runtime_configuration);
        return;
    }

    impl::Runtime::Initialize(runtime_configuration);
}

}  // namespace score::mw::com::runtime
