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
#include "score/mw/com/impl/serialization/serializer_library.h"

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace score::serialization::someip
{
namespace
{

// Stands in for the error domain of a library, which is a domain object of its own in every library.
class ForeignErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode&) const noexcept override
    {
        return "message of the foreign domain";
    }
};

constexpr ForeignErrorDomain kForeignDomain{};

score::result::Error ForeignError(const score::result::ErrorCode code)
{
    return {code, kForeignDomain, "user message of the library"};
}

constexpr auto ToCode(const SerializerErrc code) noexcept
{
    return static_cast<score::result::ErrorCode>(code);
}

constexpr std::array<SerializerErrc, 5U> kCodesOfALibrary{SerializerErrc::kBufferTooSmall,
                                                          SerializerErrc::kBufferTooLarge,
                                                          SerializerErrc::kDeserializationFailed,
                                                          SerializerErrc::kInvalidArgument,
                                                          SerializerErrc::kInternalError};

TEST(SerializerLibraryTest, ErrorOfAnotherDomainIsNeverEqualToTheErrorOfTheHost)
{
    // This is what makes the translation necessary.
    for (const auto code : kCodesOfALibrary)
    {
        EXPECT_NE(ForeignError(ToCode(code)), MakeError(code));
    }
}

TEST(SerializerLibraryTest, TranslatingAnErrorOfALibraryYieldsTheErrorOfTheHost)
{
    for (const auto code : kCodesOfALibrary)
    {
        const auto translated = detail::TranslateLibraryError(ForeignError(ToCode(code)));

        EXPECT_EQ(translated, MakeError(code));
        EXPECT_TRUE(translated.UserMessage().empty());
    }
}

TEST(SerializerLibraryTest, CodesOutsideOfTheContractOfALibraryBecomeInternalErrors)
{
    constexpr score::result::ErrorCode kUnknownCode{4711};
    const std::array<score::result::ErrorCode, 3U> codes{
        ToCode(SerializerErrc::kCreationFailed), ToCode(SerializerErrc::kIncompatibleLibrary), kUnknownCode};

    for (const auto code : codes)
    {
        EXPECT_EQ(detail::TranslateLibraryError(ForeignError(code)), MakeError(SerializerErrc::kInternalError));
    }
}

TEST(SerializerLibraryTest, OpeningAMissingLibraryFails)
{
    const auto library = SerializerLibrary::Open("/nonexistent/libmwcom_serializer_missing.so");

    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error(), MakeError(SerializerErrc::kCreationFailed));
}

}  // namespace
}  // namespace score::serialization::someip
