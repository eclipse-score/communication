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
#include "score/mw/com/impl/bindings/lola/test/service_discovery_test_resources.h"

#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"

#include "score/filesystem/path.h"

namespace score::mw::com::impl::lola
{

namespace
{

#ifdef __QNXNTO__
const filesystem::Path kTmpPath{"/tmp_discovery/mw_com_lola/service_discovery"};
#else
const filesystem::Path kTmpPath{"/tmp/mw_com_lola/service_discovery"};
#endif

}  // namespace

score::cpp::callback<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> CreateWrappedMockFindServiceHandler(
    ::testing::MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>& mock_find_service_handler)
{
    return
        [&mock_find_service_handler](ServiceHandleContainer<HandleType> containers, FindServiceHandle handle) noexcept {
            mock_find_service_handler.AsStdFunction()(containers, handle);
        };
}

FileSystemGuard::~FileSystemGuard() noexcept
{
    filesystem_.standard->RemoveAll(kTmpPath);
}

}  // namespace score::mw::com::impl::lola
