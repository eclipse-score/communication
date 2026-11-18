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
/// \brief Integration test that exercises the generated SOME/IP serialization library end to end.
///
/// The test loads libmwcom_serializer_someip.so with SerializerLibrary, exactly as mw::com does at runtime. The
/// loader verifies the ABI descriptor of the library, resolves the CreateSerializer_<structural-hash>_<attribute-hash>
/// and DestroySerializer entry points and hands out a serializer that is typed for SpeedRpm.

#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm.h"
#include "score/mw/com/impl/serialization/serializer.h"
#include "score/mw/com/impl/serialization/serializer_abi.h"
#include "score/mw/com/impl/serialization/serializer_library.h"

#include "score/result/result.h"

#include "tools/cpp/runfiles/runfiles.h"

#include <score/span.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace score::serialization::someip
{
namespace
{

using bazel::tools::cpp::runfiles::Runfiles;

constexpr const char* kLibrary = "libmwcom_serializer_someip.so";
constexpr const char* kLibraryWithoutDescriptor = "libmwcom_serializer_without_descriptor.so";

#ifndef __QNX__
constexpr const char* kRunfilesDirectory =
    "score_communication/score/mw/com/impl/bindings/someip/serialization/generated_code_example/";
#endif

constexpr std::string_view kSpeedRpmSuffix{"2c5f8d1a_7e3a91c4"};

SpeedRpm MakeSample()
{
    SpeedRpm sample{};
    sample.speed_kmh = 150.75F;
    sample.rpm = 5000U;
    sample.speed_samples = {100.0F, 125.0F, 150.75F};
    return sample;
}

void ExpectRoundTrip(const TypedSerializer<SpeedRpm>& serializer)
{
    const SpeedRpm original = MakeSample();

    const auto max_size = serializer.GetMaxSerializedSize();
    ASSERT_TRUE(max_size.has_value());
    std::vector<std::uint8_t> buffer(max_size.value());
    const auto written = serializer.Serialize(original, buffer);
    ASSERT_TRUE(written.has_value());
    ASSERT_LE(written.value(), buffer.size());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(score::cpp::span<const std::uint8_t>{buffer.data(), written.value()}, decoded)
                    .has_value());

    EXPECT_FLOAT_EQ(decoded.speed_kmh, original.speed_kmh);
    EXPECT_EQ(decoded.rpm, original.rpm);
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);
}

class SpeedRpmSerializerIntegrationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
#ifndef __QNX__
        std::string error{};
        runfiles_.reset(Runfiles::CreateForTest(&error));
        ASSERT_NE(runfiles_, nullptr) << error;
#endif
    }

    // Bazel provides the libraries as runfiles. The QNX test runner has no runfiles for shared libraries; it puts them
    // on LD_LIBRARY_PATH, where dlopen finds them by name.
    std::string PathOf(const std::string& library) const
    {
#ifdef __QNX__
        return library;
#else
        return runfiles_->Rlocation(kRunfilesDirectory + library);
#endif
    }

    // The SerializerLibrary object goes out of scope on return; only the serializer keeps the library loaded.
    score::Result<TypedSerializer<SpeedRpm>> CreateSerializerAndDropLibrary() const
    {
        const auto library = SerializerLibrary::Open(PathOf(kLibrary));
        if (!library.has_value())
        {
            return MakeUnexpected<TypedSerializer<SpeedRpm>>(library.error());
        }
        return library.value().Create<SpeedRpm>(kSpeedRpmSuffix);
    }

#ifndef __QNX__
    std::unique_ptr<Runfiles> runfiles_;
#endif
};

TEST_F(SpeedRpmSerializerIntegrationTest, RoundTripThroughLoadedLibrary)
{
    // Given the generated library
    const auto library = SerializerLibrary::Open(PathOf(kLibrary));
    ASSERT_TRUE(library.has_value()) << library.error();

    // When creating the serializer of SpeedRpm
    const auto serializer = library.value().Create<SpeedRpm>(kSpeedRpmSuffix);
    ASSERT_TRUE(serializer.has_value()) << serializer.error();

    // Then a sample survives the round trip through the serializer
    ExpectRoundTrip(serializer.value());
}

TEST_F(SpeedRpmSerializerIntegrationTest, SerializerKeepsTheLibraryLoadedAfterTheLibraryObjectIsGone)
{
    // Given a serializer created from the generated library, whose SerializerLibrary object has gone out of scope
    const auto serializer = CreateSerializerAndDropLibrary();
    ASSERT_TRUE(serializer.has_value()) << serializer.error();

    // When using the serializer
    // Then it still works
    ExpectRoundTrip(serializer.value());
}

TEST_F(SpeedRpmSerializerIntegrationTest, LibraryBuiltForAnotherAbiIsRejected)
{
    // Given an expected ABI descriptor with another ABI version than the one of the library
    SerializerAbiInfo expected = MakeSerializerAbiInfo();
    expected.abi_version += 1U;

    // When opening the library with the expected descriptor
    const auto library = SerializerLibrary::Open(PathOf(kLibrary), expected);

    // Then the library is rejected as incompatible and the user message names the mismatching field
    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error(), MakeError(SerializerErrc::kIncompatibleLibrary));
    EXPECT_EQ(library.error().UserMessage(), "abi_version");
}

TEST_F(SpeedRpmSerializerIntegrationTest, LibraryWithoutAbiDescriptorIsRejected)
{
    // Given a library that does not export an ABI descriptor
    // When opening the library
    const auto library = SerializerLibrary::Open(PathOf(kLibraryWithoutDescriptor));

    // Then the library is rejected as incompatible
    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error(), MakeError(SerializerErrc::kIncompatibleLibrary));
}

TEST_F(SpeedRpmSerializerIntegrationTest, FactoryThatTheLibraryDoesNotExportIsReported)
{
    // Given the generated library
    const auto library = SerializerLibrary::Open(PathOf(kLibrary));
    ASSERT_TRUE(library.has_value()) << library.error();

    // When creating a serializer with a suffix for which the library exports no factory
    const auto serializer = library.value().Create<SpeedRpm>("0000000_00000000");

    // Then creating the serializer fails with a creation failed error
    ASSERT_FALSE(serializer.has_value());
    EXPECT_EQ(serializer.error(), MakeError(SerializerErrc::kCreationFailed));
}

TEST_F(SpeedRpmSerializerIntegrationTest, SerializationErrorOfTheLibraryIsReportedInTheErrorDomainOfTheHost)
{
    // Given a serializer created from the generated library
    const auto serializer = CreateSerializerAndDropLibrary();
    ASSERT_TRUE(serializer.has_value()) << serializer.error();

    // When serializing a sample into a buffer that is too small
    std::array<std::uint8_t, 4U> too_small{};
    const auto written = serializer.value().Serialize(MakeSample(), too_small);

    // Then the error is the buffer too small error of the host
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error(), MakeError(SerializerErrc::kBufferTooSmall));
}

TEST_F(SpeedRpmSerializerIntegrationTest, DeserializationErrorOfTheLibraryIsReportedInTheErrorDomainOfTheHost)
{
    // Given a serializer created from the generated library
    const auto serializer = CreateSerializerAndDropLibrary();
    ASSERT_TRUE(serializer.has_value()) << serializer.error();

    // When deserializing a truncated buffer
    SpeedRpm decoded{};
    const std::array<std::uint8_t, 3U> truncated{};
    const auto read = serializer.value().Deserialize(truncated, decoded);

    // Then the error is the deserialization failed error of the host, without the user message of the library
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error(), MakeError(SerializerErrc::kDeserializationFailed));
    EXPECT_TRUE(read.error().UserMessage().empty());
}

}  // namespace
}  // namespace score::serialization::someip
