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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGES_MESSAGE_ERROR_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGES_MESSAGE_ERROR_H_

#include "score/result/result.h"

#include <score/span.hpp>

#include <string_view>

namespace score::mw::com::gateway
{

enum class MessageErrorc : score::result::ErrorCode
{
    kBufferTooSmall = 1,
    kPayloadInvalid = 2,
};

score::result::Error MakeError(const MessageErrorc code, const std::string_view message = "");

class MessageErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    {
        switch (code)
        {
            case static_cast<score::result::ErrorCode>(MessageErrorc::kBufferTooSmall):
                return "Serialization buffer too small";
            case static_cast<score::result::ErrorCode>(MessageErrorc::kPayloadInvalid):
                return "Format/layout of the message payload is invalid";
            default:
                return "unknown message error";
        }
    }
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_MESSAGES_MESSAGE_ERROR_H_
