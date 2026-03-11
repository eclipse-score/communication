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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_SERVICE_DISCOVERY_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_SERVICE_DISCOVERY_TEST_RESOURCES_H

#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"

#include "score/filesystem/filesystem_struct.h"

#include <score/callback.hpp>
#include <gmock/gmock.h>

namespace score::mw::com::impl::lola
{

// Creates an score::cpp::callback wrapper which dispatches to a pointer to a MockFunction. We do this since a MockFunction
// does not fit inside an score::cpp::callback with default capacity.
score::cpp::callback<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> CreateWrappedMockFindServiceHandler(
    ::testing::MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>& mock_find_service_handler);

class FileSystemGuard
{
  public:
    explicit FileSystemGuard(filesystem::Filesystem& filesystem) noexcept : filesystem_{filesystem} {}
    ~FileSystemGuard() noexcept;

  private:
    filesystem::Filesystem& filesystem_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_SERVICE_DISCOVERY_TEST_RESOURCES_H
