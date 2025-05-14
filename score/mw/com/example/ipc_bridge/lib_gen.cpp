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
#include "score/mw/com/example/ipc_bridge/datatype.h"
#include "score/mw/com/impl/rust/proxy_bridge_macros.h"

BEGIN_EXPORT_MW_COM_INTERFACE(mw_com_IpcBridge, ::score::mw::com::IpcBridgeProxy, ::score::mw::com::IpcBridgeSkeleton)
EXPORT_MW_COM_EVENT(mw_com_IpcBridge, ::score::mw::com::MapApiLanesStamped, map_api_lanes_stamped_)
END_EXPORT_MW_COM_INTERFACE()

EXPORT_MW_COM_TYPE(mw_com_MapApiLanesStamped, ::score::mw::com::MapApiLanesStamped)
