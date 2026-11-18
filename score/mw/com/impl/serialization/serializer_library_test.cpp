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

namespace score::mw::com::impl::serialization
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
    // Given each code that a library may report, carried by an error of the error domain of the library
    for (const auto code : kCodesOfALibrary)
    {
        // When comparing it with the error of the host that has the same code
        // Then they differ, which is what makes the translation necessary
        EXPECT_NE(ForeignError(ToCode(code)), MakeError(code));
    }
}

TEST(SerializerLibraryTest, TranslatingAnErrorOfALibraryYieldsTheErrorOfTheHost)
{
    // Given each code that a library may report, carried by an error of the error domain of the library
    for (const auto code : kCodesOfALibrary)
    {
        // When translating the error
        const auto translated = detail::TranslateLibraryError(ForeignError(ToCode(code)));

        // Then it is the error of the host with the same code and without the user message of the library
        EXPECT_EQ(translated, MakeError(code));
        EXPECT_TRUE(translated.UserMessage().empty());
    }
}

TEST(SerializerLibraryTest, CodesOutsideOfTheContractOfALibraryBecomeInternalErrors)
{
    // Given codes that a library must not report: codes of errors that only the host raises and an unknown code
    constexpr score::result::ErrorCode kUnknownCode{4711};
    const std::array<score::result::ErrorCode, 3U> codes{
        ToCode(SerializerErrc::kCreationFailed), ToCode(SerializerErrc::kIncompatibleLibrary), kUnknownCode};

    for (const auto code : codes)
    {
        // When translating an error of the library that carries the code
        // Then it becomes an internal error of the host
        EXPECT_EQ(detail::TranslateLibraryError(ForeignError(code)), MakeError(SerializerErrc::kInternalError));
    }
}

TEST(SerializerLibraryTest, OpeningAMissingLibraryFails)
{
    // Given the path of a library that does not exist
    // When opening the library
    const auto library = SerializerLibrary::Open("/nonexistent/libmwcom_serializer_missing.so");

    // Then opening fails with a creation failed error
    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error(), MakeError(SerializerErrc::kCreationFailed));
}

}  // namespace
}  // namespace score::mw::com::impl::serialization
