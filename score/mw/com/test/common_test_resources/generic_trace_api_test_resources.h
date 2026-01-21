/*******************************************************************************
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
 *******************************************************************************/

#include "score/os/mman.h"

#include "score/analysis/tracing/generic_trace_library/mock/trace_library_mock.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#include <score/expected.hpp>
#include <score/optional.hpp>

#include <gmock/gmock.h>
#include <iostream>
#include <memory>
#include <utility>

namespace score::mw::com::test
{

class TypedMemoryMock : public score::memory::shared::TypedMemory
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                AllocateNamedTypedMemory,
                (std::size_t, std::string, const score::memory::shared::permission::UserPermissions&),
                (const, noexcept, override));
    MOCK_METHOD((score::cpp::expected<int, score::os::Error>),
                AllocateAndOpenAnonymousTypedMemory,
                (std::uint64_t),
                (const, noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Unlink, (std::string_view), (const, noexcept, override));
    MOCK_METHOD((score::cpp::expected<uid_t, score::os::Error>), GetCreatorUid, (std::string_view), (const, noexcept, override));
};

struct GenericTraceApiMockContext
{
    score::analysis::tracing::TraceLibraryMock generic_trace_api_mock{};
    score::analysis::tracing::TraceDoneCallBackType stored_trace_done_cb{};
    score::cpp::optional<score::analysis::tracing::TraceContextId> last_trace_context_id{};
    std::shared_ptr<TypedMemoryMock> typed_memory_mock{};
};

void SetupGenericTraceApiMocking(GenericTraceApiMockContext& context) noexcept;

}  // namespace score::mw::com::test
