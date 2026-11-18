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
#include "score/mw/com/impl/serialization/serializer_abi.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace score::serialization::someip
{
namespace
{

struct FieldCase
{
    std::uint32_t SerializerAbiInfo::* field;
    std::string_view name;
};

constexpr std::array<FieldCase, 9U> kAllFields{{
    {&SerializerAbiInfo::struct_size, "struct_size"},
    {&SerializerAbiInfo::abi_version, "abi_version"},
    {&SerializerAbiInfo::compiler, "compiler"},
    {&SerializerAbiInfo::cxx_abi, "cxx_abi"},
    {&SerializerAbiInfo::stdlib, "stdlib"},
    {&SerializerAbiInfo::size_of_result_size_t, "size_of_result_size_t"},
    {&SerializerAbiInfo::size_of_result_void, "size_of_result_void"},
    {&SerializerAbiInfo::size_of_span, "size_of_span"},
    {&SerializerAbiInfo::size_of_error, "size_of_error"},
}};

std::string_view MismatchOf(const SerializerAbiInfo& library, const SerializerAbiInfo& host)
{
    const char* const mismatch = FindAbiMismatch(library, host);
    return mismatch == nullptr ? std::string_view{"<compatible>"} : std::string_view{mismatch};
}

TEST(SerializerAbiTest, DescriptorOfTheSameBuildMatches)
{
    const SerializerAbiInfo host = MakeSerializerAbiInfo();

    EXPECT_EQ(FindAbiMismatch(host, host), nullptr);
}

TEST(SerializerAbiTest, DescriptorDescribesTheBuild)
{
    const SerializerAbiInfo info = MakeSerializerAbiInfo();

    EXPECT_EQ(info.struct_size, sizeof(SerializerAbiInfo));
    EXPECT_EQ(info.abi_version, kSerializerAbiVersion);
    EXPECT_EQ(info.size_of_result_size_t, sizeof(score::Result<std::size_t>));
    EXPECT_EQ(info.size_of_result_void, sizeof(score::Result<void>));
    EXPECT_EQ(info.size_of_span, sizeof(score::cpp::span<std::uint8_t>));
    EXPECT_EQ(info.size_of_error, sizeof(score::result::Error));
}

TEST(SerializerAbiTest, EveryFieldIsComparedAndNamedInTheMismatch)
{
    const SerializerAbiInfo host = MakeSerializerAbiInfo();

    for (const auto& [field, name] : kAllFields)
    {
        SerializerAbiInfo library = host;
        library.*field += 1U;

        EXPECT_EQ(MismatchOf(library, host), name);
    }
}

TEST(SerializerAbiTest, FieldsAtTheFixedOffsetsAreReportedBeforeTheOthers)
{
    const SerializerAbiInfo host = MakeSerializerAbiInfo();

    // A library of another revision may be shorter than our descriptor, so its remaining fields must not be read.
    SerializerAbiInfo other_revision = host;
    other_revision.abi_version += 1U;
    other_revision.struct_size += 4U;
    other_revision.compiler += 1U;
    EXPECT_EQ(MismatchOf(other_revision, host), "abi_version");

    other_revision.abi_version = host.abi_version;
    EXPECT_EQ(MismatchOf(other_revision, host), "struct_size");
}

TEST(SerializerAbiTest, VersionAndSizeStayAtTheFirstTwoOffsets)
{
    const SerializerAbiInfo info = MakeSerializerAbiInfo();

    std::array<std::uint32_t, 2U> header{};
    static_assert(sizeof(header) <= sizeof(info));
    std::memcpy(header.data(), &info, sizeof(header));

    EXPECT_EQ(header[0U], info.struct_size);
    EXPECT_EQ(header[1U], info.abi_version);
}

}  // namespace
}  // namespace score::serialization::someip
