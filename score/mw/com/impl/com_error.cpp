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
#include "score/mw/com/impl/com_error.h"
#include "score/result/result.h"

namespace
{
constexpr score::mw::com::impl::ComErrorDomain g_comErrorDomain;
}  // namespace

namespace score::mw::com::impl
{

score::result::Error MakeError(const ComErrc code, const std::string_view message)
{
    return {static_cast<score::result::ErrorCode>(code), g_comErrorDomain, message};
}

ComErrcSerializedType SerializeSuccess()
{
    static_assert(ComErrc::kInvalid == static_cast<ComErrc>(0U),
                  "The serialization scheme uses 0 to represent 'No error'. Therefore, we cannot have a valid error "
                  "code which corresponds to an integer value 0.");
    return ComErrcSerializedType(0);
}

ComErrcSerializedType SerializeError(const ComErrc error_code)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        ((ComErrc::kInvalid < error_code) && (error_code < ComErrc::kNumEnumElements)),
        "The error code value must be within the non-inclusive range of kInvalid to "
        "kNumEnumElements. kInvalid (i.e. 0) will be used to represent 'No error' in the serialized format and "
        "kNumEnumElements is used to check that an invalid enum value is not provided. It must be manually ensured "
        "that kInvalid is the smallest enum value and kNumEnumElements is the largest");
    return static_cast<ComErrcSerializedType>(error_code);
}

score::ResultBlank Deserialize(const ComErrcSerializedType serialized_error_code)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        ((static_cast<ComErrcSerializedType>(ComErrc::kInvalid) <= serialized_error_code) &&
         (serialized_error_code < static_cast<ComErrcSerializedType>(ComErrc::kNumEnumElements))),
        "The error code value must be either kInvalid (i.e. 0) which is used to represent 'No error' or an error value "
        "up to kNumEnumElements. It must be manually ensured that kInvalid is the smallest enum value and "
        "kNumEnumElements is the largest");

    const bool contains_error = serialized_error_code != 0;
    if (contains_error)
    {
        return MakeUnexpected(static_cast<ComErrc>(serialized_error_code));
    }
    return {};
}

}  // namespace score::mw::com::impl
