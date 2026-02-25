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

namespace score::mw::com::impl
{

template <typename ErrorCode>
auto ErrorSerializer<ErrorCode>::SerializeSuccess() -> SerializedErrorType
{
    static_assert(ErrorCode::kInvalid == static_cast<ErrorCode>(0U),
                  "The serialization scheme uses 0 to represent 'No error'. Therefore, we cannot have a valid error "
                  "code which corresponds to an integer value 0.");
    return SerializedErrorType(0);
}

template <typename ErrorCode>
auto ErrorSerializer<ErrorCode>::SerializeError(const ErrorCode error_code) -> SerializedErrorType
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        ((ErrorCode::kInvalid < error_code) && (error_code < ErrorCode::kNumEnumElements)),
        "The error code value must be within the non-inclusive range of kInvalid to "
        "kNumEnumElements. kInvalid (i.e. 0) will be used to represent 'No error' in the serialized format and "
        "kNumEnumElements is used to check that an invalid enum value is not provided. It must be manually ensured "
        "that kInvalid is the smallest enum value and kNumEnumElements is the largest");
    return static_cast<SerializedErrorType>(error_code);
}

template <typename ErrorCode>
auto ErrorSerializer<ErrorCode>::Deserialize(const SerializedErrorType serialized_error_code) -> score::ResultBlank
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        ((static_cast<SerializedErrorType>(ErrorCode::kInvalid) <= serialized_error_code) &&
         (serialized_error_code < static_cast<SerializedErrorType>(ErrorCode::kNumEnumElements))),
        "The error code value must be either kInvalid (i.e. 0) which is used to represent 'No error' or an error value "
        "up to kNumEnumElements. It must be manually ensured that kInvalid is the smallest enum value and "
        "kNumEnumElements is the largest");

    const bool contains_error = serialized_error_code != 0;
    if (contains_error)
    {
        return MakeUnexpected(static_cast<ErrorCode>(serialized_error_code));
    }
    return {};
}

template class ErrorSerializer<ComErrc>;
template class ErrorSerializer<lola::MethodErrc>;

}  // namespace score::mw::com::impl
