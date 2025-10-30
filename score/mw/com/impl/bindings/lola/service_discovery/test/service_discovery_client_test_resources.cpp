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
#include "score/mw/com/impl/bindings/lola/service_discovery/test/service_discovery_client_test_resources.h"

namespace score::mw::com::impl::lola::test
{

filesystem::Path GetServiceDiscoveryPath() noexcept
{
#ifdef __QNXNTO__
    return "/tmp_discovery/mw_com_lola/service_discovery";
#else
    return "/tmp/mw_com_lola/service_discovery";
#endif
}

// Generates the file path to the service ID directory (which contains the instance ID)
filesystem::Path GenerateExpectedServiceDirectoryPath(const LolaServiceId service_id)
{
    return GetServiceDiscoveryPath() / std::to_string(static_cast<std::uint32_t>(service_id));
}

// Generates the file path to the instance ID directory (which contains the flag files)
filesystem::Path GenerateExpectedInstanceDirectoryPath(const LolaServiceId service_id,
                                                       const LolaServiceInstanceId::InstanceId instance_id)
{
    return GenerateExpectedServiceDirectoryPath(service_id) / std::to_string(static_cast<std::uint32_t>(instance_id));
}

// Creates an score::cpp::callback wrapper which dispatches to a pointer to a MockFunction. We do this since a
// MockFunction does not fit inside an score::cpp::callback with default capacity.
score::cpp::callback<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> CreateWrappedMockFindServiceHandler(
    ::testing::MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>& mock_find_service_handler)
{
    return
        [&mock_find_service_handler](ServiceHandleContainer<HandleType> containers, FindServiceHandle handle) noexcept {
            mock_find_service_handler.AsStdFunction()(containers, handle);
        };
}

}  // namespace score::mw::com::impl::lola::test
