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
#include "score/mw/com/impl/error_serializer.h"

#include "score/mw/com/impl/bindings/lola/methods/method_error.h"
#include "score/mw/com/impl/com_error.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

template <typename T>
class ErrorSerializationTypedFixture : public ::testing::Test
{
  public:
};

template <>
class ErrorSerializationTypedFixture<ComErrc> : public ::testing::Test
{
  public:
    ComErrc GetValidErrorCode()
    {
        return ComErrc::kCommunicationLinkError;
    }
};

template <>
class ErrorSerializationTypedFixture<lola::MethodErrc> : public ::testing::Test
{
  public:
    lola::MethodErrc GetValidErrorCode()
    {
        return lola::MethodErrc::kSkeletonAlreadyDestroyed;
    }
};

using AllowedErrorCodes = ::testing::Types<ComErrc, lola::MethodErrc>;
TYPED_TEST_SUITE(ErrorSerializationTypedFixture, AllowedErrorCodes, );

TYPED_TEST(ErrorSerializationTypedFixture, SerializeSuccessReturnsZero)
{
    // When calling SerializeSuccess
    const auto serialized_value = ErrorSerializer<TypeParam>::SerializeSuccess();

    // Then the serialized value should be 0
    EXPECT_EQ(serialized_value, 0);
}

TYPED_TEST(ErrorSerializationTypedFixture, SerializeErrorReturnsErrorAsInteger)
{
    // When calling SerializeError with a valid error code
    const auto error_code = this->GetValidErrorCode();
    const auto serialized_value = ErrorSerializer<TypeParam>::SerializeError(error_code);

    // Then the serialized value should contain the error code as an integer
    EXPECT_EQ(serialized_value, static_cast<std::int32_t>(error_code));
}

TYPED_TEST(ErrorSerializationTypedFixture, SerializeErrorTerminatesWhenPassingkInvalid)
{
    // When calling SerializeError with kInvalid
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ErrorSerializer<TypeParam>::SerializeError(TypeParam::kInvalid));
}

TYPED_TEST(ErrorSerializationTypedFixture, SerializeErrorTerminatesWhenPassingErrorCodeOutOfRange)
{
    // When calling SerializeError with an error code out of range
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ErrorSerializer<TypeParam>::SerializeError(TypeParam::kNumEnumElements));
}

TYPED_TEST(ErrorSerializationTypedFixture, DeserializeReturnsValidResultWhenPassingZero)
{
    // When calling Deserialize with 0
    const typename ErrorSerializer<TypeParam>::SerializedErrorType serialized_value{0};
    const auto deserialized_result = ErrorSerializer<TypeParam>::Deserialize(serialized_value);

    // Then the deserialized result should not contain an error
    EXPECT_TRUE(deserialized_result.has_value());
}

TYPED_TEST(ErrorSerializationTypedFixture, DeserializeReturnsErrorWhenPassingErrorCode)
{
    // When calling Deserialize with a valid serialized error code
    const auto error_code = this->GetValidErrorCode();
    const auto serialized_value = static_cast<typename ErrorSerializer<TypeParam>::SerializedErrorType>(error_code);
    const auto deserialized_result = ErrorSerializer<TypeParam>::Deserialize(serialized_value);

    // Then the deserialized result should contain the serialized error
    ASSERT_FALSE(deserialized_result.has_value());
    EXPECT_EQ(deserialized_result.error(), error_code);
}

TYPED_TEST(ErrorSerializationTypedFixture, DeserializeTerminatesWhenPassingIntegerOutOfRange)
{
    // When calling Deserialize with a serialized error code out of range
    // Then the program terminates
    const auto serialized_value =
        static_cast<typename ErrorSerializer<TypeParam>::SerializedErrorType>(TypeParam::kNumEnumElements);
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = ErrorSerializer<TypeParam>::Deserialize(serialized_value));
}

}  // namespace
}  // namespace score::mw::com::impl
