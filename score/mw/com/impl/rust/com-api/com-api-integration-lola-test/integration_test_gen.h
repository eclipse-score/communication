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

template <typename Trait>
class PrimitiveInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<U8Data> u8_event{*this, "u8_event"};
    typename Trait::template Event<U16Data> u16_event{*this, "u16_event"};
    typename Trait::template Event<U32Data> u32_event{*this, "u32_event"};
    typename Trait::template Event<U64Data> u64_event{*this, "u64_event"};
    typename Trait::template Event<I8Data> i8_event{*this, "i8_event"};
    typename Trait::template Event<I16Data> i16_event{*this, "i16_event"};
    typename Trait::template Event<I32Data> i32_event{*this, "i32_event"};
    typename Trait::template Event<I64Data> i64_event{*this, "i64_event"};
    typename Trait::template Event<F32Data> f32_event{*this, "f32_event"};
    typename Trait::template Event<F64Data> f64_event{*this, "f64_event"};
    typename Trait::template Event<BoolData> bool_event{*this, "bool_event"};
};

using PrimitiveProxy = ::score::mw::com::AsProxy<PrimitiveInterface>;
using PrimitiveSkeleton = ::score::mw::com::AsSkeleton<PrimitiveInterface>;

template <typename Trait>
class UserDefinedInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<SimpleStruct> simple_event{*this, "simple_event"};
    typename Trait::template Event<ComplexStruct> complex_event{*this, "complex_event"};
    typename Trait::template Event<NestedStruct> nested_event{*this, "nested_event"};
    typename Trait::template Event<Point> point_event{*this, "point_event"};
    typename Trait::template Event<Point3D> point3d_event{*this, "point3d_event"};
    typename Trait::template Event<SensorData> sensor_event{*this, "sensor_event"};
    typename Trait::template Event<VehicleState> vehicle_event{*this, "vehicle_event"};
};

using UserDefinedProxy = ::score::mw::com::AsProxy<UserDefinedInterface>;
using UserDefinedSkeleton = ::score::mw::com::AsSkeleton<UserDefinedInterface>;

}  // namespace score::mw::com::integration_test

#endif  // SCORE_MW_COM_IMPL_RUST_COM_API_INTEGRATION_TEST_LOLA_INTEGRATION_TEST_GEN_H
