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

#include "score/mw/com/test/com_api/bigdata_com_api_gen.h"
#include "score/mw/com/impl/rust/com-api/com-api-ffi-lola/registry_bridge_macro.h"

// Register the BigData interface with the com-api FFI bridge.
BEGIN_EXPORT_MW_COM_INTERFACE(BigDataInterface,
                              ::score::mw::com::test::BigDataProxy,
                              ::score::mw::com::test::BigDataSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::test::MapApiLanesStamped, map_api_lanes_stamped_)
EXPORT_MW_COM_EVENT(::score::mw::com::test::DummyDataStamped, dummy_data_stamped_)
END_EXPORT_MW_COM_INTERFACE()

// Export data types so that the Rust-side CommData::ID can resolve them.
EXPORT_MW_COM_TYPE(MapApiLanesStamped, ::score::mw::com::test::MapApiLanesStamped)
EXPORT_MW_COM_TYPE(DummyDataStamped, ::score::mw::com::test::DummyDataStamped)
