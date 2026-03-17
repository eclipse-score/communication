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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_RESOURCES_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_RESOURCES_H

#include "score/filesystem/path.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"

#include <score/callback.hpp>

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola::test
{

filesystem::Path GetServiceDiscoveryPath() noexcept;

// Generates the file path to the service ID directory (which contains the instance ID)
filesystem::Path GenerateExpectedServiceDirectoryPath(const LolaServiceId service_id);

// Generates the file path to the instance ID directory (which contains the flag files)
filesystem::Path GenerateExpectedInstanceDirectoryPath(const LolaServiceId service_id,
                                                       const LolaServiceInstanceId::InstanceId instance_id);

// Creates an score::cpp::callback wrapper which dispatches to a pointer to a MockFunction. We do this since a
// MockFunction does not fit inside an score::cpp::callback with default capacity.
score::cpp::callback<void(ServiceHandleContainer<HandleType>, FindServiceHandle)> CreateWrappedMockFindServiceHandler(
    ::testing::MockFunction<void(ServiceHandleContainer<HandleType>, FindServiceHandle)>& mock_find_service_handler);

}  // namespace score::mw::com::impl::lola::test

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_RESOURCES_H
