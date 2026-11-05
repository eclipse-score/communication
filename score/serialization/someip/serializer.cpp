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
#include "score/serialization/someip/serializer.h"

namespace score::serialization::someip
{

namespace
{

class SerializerErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    {
        switch (code)
        {
            case static_cast<score::result::ErrorCode>(SerializerErrc::kBufferTooSmall):
                return "Output buffer too small for serialized data.";
            case static_cast<score::result::ErrorCode>(SerializerErrc::kBufferTooLarge):
                return "Input buffer larger than expected.";
            case static_cast<score::result::ErrorCode>(SerializerErrc::kDeserializationFailed):
                return "Deserialization failed; buffer is malformed.";
            case static_cast<score::result::ErrorCode>(SerializerErrc::kUnknownDatatypeHash):
                return "Datatype hash not known to this serializer library.";
            case static_cast<score::result::ErrorCode>(SerializerErrc::kCreationFailed):
                return "Serializer could not be created.";
            case static_cast<score::result::ErrorCode>(SerializerErrc::kInvalidArgument):
                return "Invalid argument passed to serializer.";
            case static_cast<score::result::ErrorCode>(SerializerErrc::kInternalError):
                return "Internal serializer error.";
            default:
                return "unknown serializer error";
        }
    }
};

constexpr SerializerErrorDomain g_serializer_error_domain;

}  // namespace

score::result::Error MakeError(const SerializerErrc code, const std::string_view message)
{
    return {static_cast<score::result::ErrorCode>(code), g_serializer_error_domain, message};
}

}  // namespace score::serialization::someip
