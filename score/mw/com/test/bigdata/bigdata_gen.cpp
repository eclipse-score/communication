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

#include "score/mw/com/impl/rust/bridge_macros.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"

BEGIN_EXPORT_MW_COM_INTERFACE(mw_com_test_BigData,
                              ::score::mw::com::test::BigDataProxy,
                              ::score::mw::com::test::BigDataSkeleton)
EXPORT_MW_COM_EVENT(mw_com_test_BigData, ::score::mw::com::test::MapApiLanesStamped, map_api_lanes_stamped_)
EXPORT_MW_COM_EVENT(mw_com_test_BigData, ::score::mw::com::test::DummyDataStamped, dummy_data_stamped_)
END_EXPORT_MW_COM_INTERFACE()

EXPORT_MW_COM_TYPE(mw_com_test_MapApiLanesStamped, ::score::mw::com::test::MapApiLanesStamped)
EXPORT_MW_COM_TYPE(mw_com_test_DummyDataStamped, ::score::mw::com::test::DummyDataStamped)
