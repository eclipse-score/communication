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

BEGIN_EXPORT_MW_COM_INTERFACE(U8Interface,
                              ::score::mw::com::integration_test::U8Proxy,
                              ::score::mw::com::integration_test::U8Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U8Data, u8_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(U16Interface,
                              ::score::mw::com::integration_test::U16Proxy,
                              ::score::mw::com::integration_test::U16Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U16Data, u16_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(U32Interface,
                              ::score::mw::com::integration_test::U32Proxy,
                              ::score::mw::com::integration_test::U32Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U32Data, u32_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(U64Interface,
                              ::score::mw::com::integration_test::U64Proxy,
                              ::score::mw::com::integration_test::U64Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::U64Data, u64_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(I8Interface,
                              ::score::mw::com::integration_test::I8Proxy,
                              ::score::mw::com::integration_test::I8Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I8Data, i8_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(I16Interface,
                              ::score::mw::com::integration_test::I16Proxy,
                              ::score::mw::com::integration_test::I16Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I16Data, i16_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(I32Interface,
                              ::score::mw::com::integration_test::I32Proxy,
                              ::score::mw::com::integration_test::I32Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I32Data, i32_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(I64Interface,
                              ::score::mw::com::integration_test::I64Proxy,
                              ::score::mw::com::integration_test::I64Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::I64Data, i64_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(F32Interface,
                              ::score::mw::com::integration_test::F32Proxy,
                              ::score::mw::com::integration_test::F32Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::F32Data, f32_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(F64Interface,
                              ::score::mw::com::integration_test::F64Proxy,
                              ::score::mw::com::integration_test::F64Skeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::F64Data, f64_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(BoolInterface,
                              ::score::mw::com::integration_test::BoolProxy,
                              ::score::mw::com::integration_test::BoolSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::BoolData, bool_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(SimpleStructInterface,
                              ::score::mw::com::integration_test::SimpleStructProxy,
                              ::score::mw::com::integration_test::SimpleStructSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::SimpleStruct, simple_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(ComplexStructInterface,
                              ::score::mw::com::integration_test::ComplexStructProxy,
                              ::score::mw::com::integration_test::ComplexStructSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::ComplexStruct, complex_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(NestedStructInterface,
                              ::score::mw::com::integration_test::NestedStructProxy,
                              ::score::mw::com::integration_test::NestedStructSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::NestedStruct, nested_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(PointInterface,
                              ::score::mw::com::integration_test::PointProxy,
                              ::score::mw::com::integration_test::PointSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::Point, point_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(Point3DInterface,
                              ::score::mw::com::integration_test::Point3DProxy,
                              ::score::mw::com::integration_test::Point3DSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::Point3D, point3d_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(SensorDataInterface,
                              ::score::mw::com::integration_test::SensorDataProxy,
                              ::score::mw::com::integration_test::SensorDataSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::SensorData, sensor_event)
END_EXPORT_MW_COM_INTERFACE()

BEGIN_EXPORT_MW_COM_INTERFACE(VehicleStateInterface,
                              ::score::mw::com::integration_test::VehicleStateProxy,
                              ::score::mw::com::integration_test::VehicleStateSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::VehicleState, vehicle_event)
END_EXPORT_MW_COM_INTERFACE()

// Export all types
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
