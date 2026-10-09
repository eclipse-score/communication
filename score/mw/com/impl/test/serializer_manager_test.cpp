/********************************************************************************
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
 ********************************************************************************/

/// \file
/// \brief Unit tests for SerializerManager.
///
/// The manager dlopens the generated serialization library (libmwcom_serializer_someip.so) by soname and
/// resolves the CreateSerializer / DestroySerializer factory symbols with dlsym. Because the outcome
/// depends on the dynamic loader's environment - which is fixed once per process - the three scenarios are
/// grouped into three test suites that a single BUILD target each runs in isolation via --gtest_filter:
///
///  * SerializerManagerTest              - the real generated .so is on LD_LIBRARY_PATH, so dlopen and
///                                         dlsym succeed (happy path, caching, unknown datatype after a
///                                         successful load).
///  * SerializerManagerLoadFailureTest   - no library on the search path, so dlopen fails
///                                         (SerializerErrc::kCreationFailed).
///  * SerializerManagerSymbolFailureTest - a stub library with the right soname but without the factory
///                                         symbols is on the search path, so dlopen succeeds but dlsym
///                                         fails (SerializerErrc::kCreationFailed).

#include "score/mw/com/impl/serializer_manager.h"

#include "score/serialization/someip/generated_code_example/speed_rpm.h"
#include "score/serialization/someip/serializer.h"

#include "score/result/result.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using serialization::someip::ISerializer;
using serialization::someip::SerializerErrc;
using serialization::someip::SpeedRpm;

// A datatype hash that is not known to the example serializer library.
constexpr std::uint64_t kUnknownDatatypeHash = 0xDEADBEEFULL;

// Any datatype hash triggers a library load; in the failure suites the load or symbol resolution fails
// before the hash is ever looked up.
constexpr std::uint64_t kAnyDatatypeHash = 0x2c5f8d1aULL;

// -----------------------------------------------------------------------------------------------------
// Successful-load suite: the real generated library is discoverable via LD_LIBRARY_PATH (see BUILD), so
// dlopen and dlsym both succeed.
// -----------------------------------------------------------------------------------------------------

TEST(SerializerManagerTest, GetSerializerReturnsWorkingSerializerForKnownDatatype)
{
    SerializerManager manager{};

    const auto serializer_result = manager.GetSerializer(SpeedRpm::kHashId);
    ASSERT_TRUE(serializer_result.has_value());

    const ISerializer& serializer = serializer_result.value();

    SpeedRpm original{};
    original.speed_kmh = 150.75F;
    original.rpm = 5000U;
    original.speed_samples = {100.0F, 125.0F, 150.75F};

    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());

    EXPECT_FLOAT_EQ(decoded.speed_kmh, original.speed_kmh);
    EXPECT_EQ(decoded.rpm, original.rpm);
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);
}

TEST(SerializerManagerTest, GetSerializerReturnsCachedInstanceForSameDatatype)
{
    SerializerManager manager{};

    const auto first = manager.GetSerializer(SpeedRpm::kHashId);
    ASSERT_TRUE(first.has_value());

    const auto second = manager.GetSerializer(SpeedRpm::kHashId);
    ASSERT_TRUE(second.has_value());

    // The manager must hand out exactly one serializer per datatype hash and reuse it afterwards.
    EXPECT_EQ(&first.value().get(), &second.value().get());
}

TEST(SerializerManagerTest, GetSerializerReturnsErrorForUnknownDatatype)
{
    SerializerManager manager{};

    const auto result = manager.GetSerializer(kUnknownDatatypeHash);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kUnknownDatatypeHash));
}

TEST(SerializerManagerTest, GetSerializerReturnsUnknownDatatypeWithoutReloadingLibrary)
{
    SerializerManager manager{};

    // First load the library successfully by requesting a known datatype.
    const auto known = manager.GetSerializer(SpeedRpm::kHashId);
    ASSERT_TRUE(known.has_value());

    // A subsequent unknown datatype on the SAME manager must reuse the already-loaded library
    // (EnsureLibraryLoaded early-returns instead of dlopen'ing again) and fail only because the datatype
    // hash is unknown.
    const auto unknown = manager.GetSerializer(kUnknownDatatypeHash);
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(*unknown.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kUnknownDatatypeHash));

    // The library must still be loaded afterwards: the known serializer remains available and identical.
    const auto known_again = manager.GetSerializer(SpeedRpm::kHashId);
    ASSERT_TRUE(known_again.has_value());
    EXPECT_EQ(&known.value().get(), &known_again.value().get());
}

// -----------------------------------------------------------------------------------------------------
// Load-failure suite: no library is on the loader's search path (the BUILD target omits the data
// dependency and LD_LIBRARY_PATH), so dlopen fails.
// -----------------------------------------------------------------------------------------------------

TEST(SerializerManagerLoadFailureTest, GetSerializerReturnsCreationFailedWhenLibraryCannotBeLoaded)
{
    SerializerManager manager{};

    const auto result = manager.GetSerializer(kAnyDatatypeHash);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed));
}

TEST(SerializerManagerLoadFailureTest, GetSerializerKeepsFailingOnRepeatedCallsWhenLibraryUnavailable)
{
    SerializerManager manager{};

    const auto first = manager.GetSerializer(kAnyDatatypeHash);
    const auto second = manager.GetSerializer(kAnyDatatypeHash);

    ASSERT_FALSE(first.has_value());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(*first.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed));
    EXPECT_EQ(*second.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed));
}

// -----------------------------------------------------------------------------------------------------
// Symbol-failure suite: a stub library carrying the expected soname but exporting none of the factory
// symbols is on the search path (see BUILD), so dlopen succeeds while dlsym fails.
// -----------------------------------------------------------------------------------------------------

TEST(SerializerManagerSymbolFailureTest, GetSerializerReturnsCreationFailedWhenFactorySymbolsMissing)
{
    SerializerManager manager{};

    const auto result = manager.GetSerializer(kAnyDatatypeHash);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed));
}

TEST(SerializerManagerSymbolFailureTest, GetSerializerKeepsFailingOnRepeatedCallsWhenFactorySymbolsMissing)
{
    SerializerManager manager{};

    const auto first = manager.GetSerializer(kAnyDatatypeHash);
    const auto second = manager.GetSerializer(kAnyDatatypeHash);

    ASSERT_FALSE(first.has_value());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(*first.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed));
    EXPECT_EQ(*second.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed));
}

}  // namespace
}  // namespace score::mw::com::impl
