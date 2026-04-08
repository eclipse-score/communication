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

#ifndef SCORE_MW_COM_TEST_BASIC_RUST_API_COM_API_GEN_H
#define SCORE_MW_COM_TEST_BASIC_RUST_API_COM_API_GEN_H

// Re-export the BigData interface types (BigDataProxy / BigDataSkeleton) that are
// already defined in big_datatype.h
#include "score/mw/com/test/common_test_resources/big_datatype.h"

namespace score::mw::com::test
{
/// Combined payload carrying every primitive type in a single struct.
/// Field order follows descending alignment to eliminate padding bytes.
struct MixedPrimitivesPayload
{
    uint64_t u64_val;
    int64_t i64_val;
    uint32_t u32_val;
    int32_t i32_val;
    float f32_val;
    uint16_t u16_val;
    int16_t i16_val;
    uint8_t u8_val;
    int8_t i8_val;
    bool flag;
};

struct SimpleStruct
{
    uint32_t id;
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
    bool is_running;
    uint32_t mileage;
};

struct ArrayStruct
{
    uint32_t values[5];
};

struct ComplexStruct
{
    uint32_t count;
    SimpleStruct simple;
    NestedStruct nested;
    Point point;
    Point3D point3d;
    SensorData sensor;
    VehicleState vehicle;
    ArrayStruct array;
};

template <typename Trait>
class MixedPrimitivesInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;
    typename Trait::template Event<MixedPrimitivesPayload> mixed_event{*this, "mixed_event"};
};

template <typename Trait>
class ComplexStructInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;
    typename Trait::template Event<ComplexStruct> complex_event{*this, "complex_event"};
};

// Type aliases for proxy and skeleton
using MixedPrimitivesProxy = ::score::mw::com::AsProxy<MixedPrimitivesInterface>;
using MixedPrimitivesSkeleton = ::score::mw::com::AsSkeleton<MixedPrimitivesInterface>;
using ComplexStructProxy = ::score::mw::com::AsProxy<ComplexStructInterface>;
using ComplexStructSkeleton = ::score::mw::com::AsSkeleton<ComplexStructInterface>;
}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_BASIC_RUST_API_COM_API_GEN_H
