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

BEGIN_EXPORT_MW_COM_INTERFACE(MixedPrimitivesInterface,
                              ::score::mw::com::integration_test::MixedPrimitivesProxy,
                              ::score::mw::com::integration_test::MixedPrimitivesSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::integration_test::MixedPrimitivesPayload, mixed_event)
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
EXPORT_MW_COM_TYPE(MixedPrimitivesPayload, ::score::mw::com::integration_test::MixedPrimitivesPayload)
EXPORT_MW_COM_TYPE(SimpleStruct, ::score::mw::com::integration_test::SimpleStruct)
EXPORT_MW_COM_TYPE(ComplexStruct, ::score::mw::com::integration_test::ComplexStruct)
EXPORT_MW_COM_TYPE(NestedStruct, ::score::mw::com::integration_test::NestedStruct)
EXPORT_MW_COM_TYPE(Point, ::score::mw::com::integration_test::Point)
EXPORT_MW_COM_TYPE(Point3D, ::score::mw::com::integration_test::Point3D)
EXPORT_MW_COM_TYPE(SensorData, ::score::mw::com::integration_test::SensorData)
EXPORT_MW_COM_TYPE(VehicleState, ::score::mw::com::integration_test::VehicleState)
