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

struct U8Data
{
    uint8_t value;
};

struct U16Data
{
    uint16_t value;
};

struct U32Data
{
    uint32_t value;
};

struct U64Data
{
    uint64_t value;
};

struct I8Data
{
    int8_t value;
};

struct I16Data
{
    int16_t value;
};

struct I32Data
{
    int32_t value;
};

struct I64Data
{
    int64_t value;
};

struct F32Data
{
    float value;
};

struct F64Data
{
    double value;
};

struct BoolData
{
    bool value;
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

// Primitive type interfaces (one per type)
CREATE_INTERFACE(U8Interface, U8Data, u8_event)
CREATE_INTERFACE(U16Interface, U16Data, u16_event)
CREATE_INTERFACE(U32Interface, U32Data, u32_event)
CREATE_INTERFACE(U64Interface, U64Data, u64_event)
CREATE_INTERFACE(I8Interface, I8Data, i8_event)
CREATE_INTERFACE(I16Interface, I16Data, i16_event)
CREATE_INTERFACE(I32Interface, I32Data, i32_event)
CREATE_INTERFACE(I64Interface, I64Data, i64_event)
CREATE_INTERFACE(F32Interface, F32Data, f32_event)
CREATE_INTERFACE(F64Interface, F64Data, f64_event)
CREATE_INTERFACE(BoolInterface, BoolData, bool_event)

// User-defined type interfaces (one per type)
CREATE_INTERFACE(SimpleStructInterface, SimpleStruct, simple_event)
CREATE_INTERFACE(ComplexStructInterface, ComplexStruct, complex_event)
CREATE_INTERFACE(NestedStructInterface, NestedStruct, nested_event)
CREATE_INTERFACE(PointInterface, Point, point_event)
CREATE_INTERFACE(Point3DInterface, Point3D, point3d_event)
CREATE_INTERFACE(SensorDataInterface, SensorData, sensor_event)
CREATE_INTERFACE(VehicleStateInterface, VehicleState, vehicle_event)

// Type aliases for proxy and skeleton
using U8Proxy = ::score::mw::com::AsProxy<U8Interface>;
using U8Skeleton = ::score::mw::com::AsSkeleton<U8Interface>;
using U16Proxy = ::score::mw::com::AsProxy<U16Interface>;
using U16Skeleton = ::score::mw::com::AsSkeleton<U16Interface>;
using U32Proxy = ::score::mw::com::AsProxy<U32Interface>;
using U32Skeleton = ::score::mw::com::AsSkeleton<U32Interface>;
using U64Proxy = ::score::mw::com::AsProxy<U64Interface>;
using U64Skeleton = ::score::mw::com::AsSkeleton<U64Interface>;
using I8Proxy = ::score::mw::com::AsProxy<I8Interface>;
using I8Skeleton = ::score::mw::com::AsSkeleton<I8Interface>;
using I16Proxy = ::score::mw::com::AsProxy<I16Interface>;
using I16Skeleton = ::score::mw::com::AsSkeleton<I16Interface>;
using I32Proxy = ::score::mw::com::AsProxy<I32Interface>;
using I32Skeleton = ::score::mw::com::AsSkeleton<I32Interface>;
using I64Proxy = ::score::mw::com::AsProxy<I64Interface>;
using I64Skeleton = ::score::mw::com::AsSkeleton<I64Interface>;
using F32Proxy = ::score::mw::com::AsProxy<F32Interface>;
using F32Skeleton = ::score::mw::com::AsSkeleton<F32Interface>;
using F64Proxy = ::score::mw::com::AsProxy<F64Interface>;
using F64Skeleton = ::score::mw::com::AsSkeleton<F64Interface>;
using BoolProxy = ::score::mw::com::AsProxy<BoolInterface>;
using BoolSkeleton = ::score::mw::com::AsSkeleton<BoolInterface>;
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
