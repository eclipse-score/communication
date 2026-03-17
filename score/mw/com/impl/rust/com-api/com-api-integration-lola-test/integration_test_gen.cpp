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

#include "integration_test_gen.h"
#include "score/mw/com/impl/rust/com-api/com-api-ffi-lola/registry_bridge_macro.h"

BEGIN_EXPORT_MW_COM_INTERFACE(PrimitiveInterface,
                              ::score::mw::com::integration_test::PrimitiveProxy,
                              ::score::mw::com::integration_test::PrimitiveSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U8Data, u8_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U16Data, u16_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U32Data, u32_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U64Data, u64_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I8Data, i8_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I16Data, i16_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I32Data, i32_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I64Data, i64_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::F32Data, f32_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::F64Data, f64_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::BoolData, bool_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(UserDefinedInterface,
                              ::score::mw::com::integration_test::UserDefinedProxy,
                              ::score::mw::com::integration_test::UserDefinedSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::SimpleStruct, simple_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::ComplexStruct, complex_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::NestedStruct, nested_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::Point, point_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::Point3D, point3d_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::SensorData, sensor_event)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::VehicleState, vehicle_event)
END_EXPORT_MW_COM_INTERFACE()

EXPORT_MW_COM_TYPE(U8Data, ::score::mw::com::integration_test::U8Data)
EXPORT_MW_COM_TYPE(U16Data, ::score::mw::com::integration_test::U16Data)
EXPORT_MW_COM_TYPE(U32Data, ::score::mw::com::integration_test::U32Data)
EXPORT_MW_COM_TYPE(U64Data, ::score::mw::com::integration_test::U64Data)
EXPORT_MW_COM_TYPE(I8Data, ::score::mw::com::integration_test::I8Data)
EXPORT_MW_COM_TYPE(I16Data, ::score::mw::com::integration_test::I16Data)
EXPORT_MW_COM_TYPE(I32Data, ::score::mw::com::integration_test::I32Data)
EXPORT_MW_COM_TYPE(I64Data, ::score::mw::com::integration_test::I64Data)
EXPORT_MW_COM_TYPE(F32Data, ::score::mw::com::integration_test::F32Data)
EXPORT_MW_COM_TYPE(F64Data, ::score::mw::com::integration_test::F64Data)
EXPORT_MW_COM_TYPE(BoolData, ::score::mw::com::integration_test::BoolData)
EXPORT_MW_COM_TYPE(SimpleStruct, ::score::mw::com::integration_test::SimpleStruct)
EXPORT_MW_COM_TYPE(ComplexStruct, ::score::mw::com::integration_test::ComplexStruct)
EXPORT_MW_COM_TYPE(NestedStruct, ::score::mw::com::integration_test::NestedStruct)
EXPORT_MW_COM_TYPE(Point, ::score::mw::com::integration_test::Point)
EXPORT_MW_COM_TYPE(Point3D, ::score::mw::com::integration_test::Point3D)
EXPORT_MW_COM_TYPE(SensorData, ::score::mw::com::integration_test::SensorData)
EXPORT_MW_COM_TYPE(VehicleState, ::score::mw::com::integration_test::VehicleState)
