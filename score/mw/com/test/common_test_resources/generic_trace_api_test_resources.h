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

#include "score/os/mman.h"

#include "platform/aas/analysis/tracing/library/generic_trace_api/mocks/trace_library_mock.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#include <amp_expected.hpp>
#include <amp_optional.hpp>

#include <gmock/gmock.h>
#include <iostream>
#include <memory>
#include <utility>

namespace bmw
{
  namespace mw
  {
    namespace com
    {
      namespace test
      {

        class TypedMemoryMock : public bmw::memory::shared::TypedMemory
        {
        public:
          MOCK_METHOD(amp::expected_blank<bmw::os::Error>,
                      AllocateNamedTypedMemory,
                      (std::size_t, std::string, const bmw::memory::shared::permission::UserPermissions &),
                      (const, noexcept, override));
          MOCK_METHOD((amp::expected<int, bmw::os::Error>),
                      AllocateAndOpenAnonymousTypedMemory,
                      (std::uint64_t),
                      (const, noexcept, override));
        };

        struct GenericTraceApiMockContext
        {
          bmw::analysis::tracing::TraceLibraryMock generic_trace_api_mock{};
          bmw::analysis::tracing::TraceDoneCallBackType stored_trace_done_cb{};
          amp::optional<bmw::analysis::tracing::TraceContextId> last_trace_context_id{};
          std::shared_ptr<TypedMemoryMock> typed_memory_mock{};
        };

        void SetupGenericTraceApiMocking(GenericTraceApiMockContext &context) noexcept;

      } // namespace test
    } // namespace com
  } // namespace mw
} // namespace bmw
