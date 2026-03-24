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

#ifndef SCORE_MW_COM_IMPL_RUST_COM_API_INTEGRATION_TEST_LOLA_INTEGRATION_TEST_GEN_H
#define SCORE_MW_COM_IMPL_RUST_COM_API_INTEGRATION_TEST_LOLA_INTEGRATION_TEST_GEN_H

#include "score/mw/com/types.h"

#include <cstddef>
#include <cstdint>

namespace score::mw::com::integration_test
{

/// Combined payload carrying every primitive type in a single struct.
/// Field order follows descending alignment to eliminate padding bytes.
struct MixedPrimitivesPayload
{
    uint64_t u64_val;
    int64_t  i64_val;
    uint32_t u32_val;
    int32_t  i32_val;
    float    f32_val;
    uint16_t u16_val;
    int16_t  i16_val;
    uint8_t  u8_val;
    int8_t   i8_val;
    bool     flag;
};

struct SimpleStruct
{
    uint32_t id;
};

struct ComplexStruct
{
    uint32_t count;
    float temperature;
    uint8_t is_active;
    uint64_t timestamp;
};

struct NestedStruct
{
    uint32_t id;
    SimpleStruct simple;
    float value;
};

struct Point
{
    float x;
    float y;
};

struct Point3D
{
    float x;
    float y;
    float z;
};

struct SensorData
{
    uint16_t sensor_id;
    float temperature;
    float humidity;
    float pressure;
};

struct VehicleState
{
    float speed;
    uint16_t rpm;
    float fuel_level;
    uint8_t is_running;
    uint32_t mileage;
};

// Macro to generate interface classes
#define CREATE_INTERFACE(ClassName, DataType, EventName)                       \
    template <typename Trait>                                                  \
    class ClassName : public Trait::Base                                       \
    {                                                                          \
      public:                                                                  \
        using Trait::Base::Base;                                               \
        typename Trait::template Event<DataType> EventName{*this, #EventName}; \
    };

// Combined primitive payload interface
CREATE_INTERFACE(MixedPrimitivesInterface, MixedPrimitivesPayload, mixed_event)

// User-defined type interfaces (one per type)
CREATE_INTERFACE(SimpleStructInterface, SimpleStruct, simple_event)
CREATE_INTERFACE(ComplexStructInterface, ComplexStruct, complex_event)
CREATE_INTERFACE(NestedStructInterface, NestedStruct, nested_event)
CREATE_INTERFACE(PointInterface, Point, point_event)
CREATE_INTERFACE(Point3DInterface, Point3D, point3d_event)
CREATE_INTERFACE(SensorDataInterface, SensorData, sensor_event)
CREATE_INTERFACE(VehicleStateInterface, VehicleState, vehicle_event)

// Type aliases for proxy and skeleton
using MixedPrimitivesProxy      = ::score::mw::com::AsProxy<MixedPrimitivesInterface>;
using MixedPrimitivesSkeleton   = ::score::mw::com::AsSkeleton<MixedPrimitivesInterface>;
using SimpleStructProxy = ::score::mw::com::AsProxy<SimpleStructInterface>;
using SimpleStructSkeleton = ::score::mw::com::AsSkeleton<SimpleStructInterface>;
using ComplexStructProxy = ::score::mw::com::AsProxy<ComplexStructInterface>;
using ComplexStructSkeleton = ::score::mw::com::AsSkeleton<ComplexStructInterface>;
using NestedStructProxy = ::score::mw::com::AsProxy<NestedStructInterface>;
using NestedStructSkeleton = ::score::mw::com::AsSkeleton<NestedStructInterface>;
using PointProxy = ::score::mw::com::AsProxy<PointInterface>;
using PointSkeleton = ::score::mw::com::AsSkeleton<PointInterface>;
using Point3DProxy = ::score::mw::com::AsProxy<Point3DInterface>;
using Point3DSkeleton = ::score::mw::com::AsSkeleton<Point3DInterface>;
using SensorDataProxy = ::score::mw::com::AsProxy<SensorDataInterface>;
using SensorDataSkeleton = ::score::mw::com::AsSkeleton<SensorDataInterface>;
using VehicleStateProxy = ::score::mw::com::AsProxy<VehicleStateInterface>;
using VehicleStateSkeleton = ::score::mw::com::AsSkeleton<VehicleStateInterface>;
}  // namespace score::mw::com::integration_test

#endif  // SCORE_MW_COM_IMPL_RUST_COM_API_INTEGRATION_TEST_LOLA_INTEGRATION_TEST_GEN_H
