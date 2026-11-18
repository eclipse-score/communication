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
/// The test loads libmwcom_serializer_someip.so with dlopen, resolves the CreateSerializer_<structural-hash>_<attribute-hash>
/// and DestroySerializer factory symbols with dlsym, serializes a SpeedRpm sample, deserializes it back
/// and asserts the round-trip preserves the data, exactly as mw::com uses the library at runtime.

#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm.h"
#include "score/mw/com/impl/serialization/serializer.h"

#include "tools/cpp/runfiles/runfiles.h"

#include <score/span.hpp>

#include <gtest/gtest.h>

#include <dlfcn.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace score::serialization::someip
{
namespace
{

using bazel::tools::cpp::runfiles::Runfiles;

using CreateSerializerFn = ISerializer* (*)() noexcept;
using DestroySerializerFn = void (*)(ISerializer*) noexcept;

constexpr const char* kLibraryRunfile =
    "score_communication/score/mw/com/impl/bindings/someip/serialization/generated_code_example/"
    "libmwcom_serializer_someip.so";

TEST(SpeedRpmSerializerIntegration, RoundTripThroughDlopenedLibrary)
{
    std::string runfiles_error{};
    const std::unique_ptr<Runfiles> runfiles{Runfiles::CreateForTest(&runfiles_error)};
    ASSERT_NE(runfiles, nullptr) << runfiles_error;

    const std::string library_path = runfiles->Rlocation(kLibraryRunfile);
    ASSERT_FALSE(library_path.empty());

    // Open the generated serialization library exactly as mw::com would.
    void* const library = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    ASSERT_NE(library, nullptr) << dlerror();

    // Resolve the factory symbols with dlsym.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): required for dlsym results.
    const auto create = reinterpret_cast<CreateSerializerFn>(dlsym(library, "CreateSerializer_2c5f8d1a_7e3a91c4"));
    ASSERT_NE(create, nullptr) << dlerror();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): required for dlsym results.
    const auto destroy = reinterpret_cast<DestroySerializerFn>(dlsym(library, "DestroySerializer"));
    ASSERT_NE(destroy, nullptr) << dlerror();

    ISerializer* const serializer = create();
    ASSERT_NE(serializer, nullptr);

    SpeedRpm original{};
    original.speed_kmh = 150.75F;
    original.rpm = 5000U;
    original.speed_samples = {100.0F, 125.0F, 150.75F};

    std::vector<std::uint8_t> buffer(serializer->GetMaxSerializedSize().value());
    const auto written = serializer->Serialize(&original, buffer);
    ASSERT_TRUE(written.has_value());
    ASSERT_LE(written.value(), buffer.size());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer->Deserialize(score::cpp::span<const std::uint8_t>{buffer.data(), written.value()}, &decoded).has_value());

    EXPECT_FLOAT_EQ(decoded.speed_kmh, original.speed_kmh);
    EXPECT_EQ(decoded.rpm, original.rpm);
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);

    destroy(serializer);

    ASSERT_EQ(dlclose(library), 0) << dlerror();
}

}  // namespace
}  // namespace score::serialization::someip
