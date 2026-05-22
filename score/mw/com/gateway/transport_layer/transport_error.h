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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_ERROR_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_ERROR_H

#include "score/result/result.h"

#include <string_view>

namespace score::mw::com::gateway
{

enum class TransportErrorc : score::result::ErrorCode
{
    kServerSetupFailed = 1,
    kServerListeningReturned,
    kClientSetupFailed,
    kFailedToProvideService,
    kConnectionFailure,
    kNotConnected,
    kTimeout,
    kInvalidMessage,
    kNotSupported,
    kSendFailure,
    kReceiveFailure,
};

score::result::Error MakeError(const TransportErrorc code, const std::string_view message = "");

class TransportErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    {
        switch (code)
        {
            case static_cast<score::result::ErrorCode>(TransportErrorc::kServerSetupFailed):
                return "Gateway server could not be setup.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kServerListeningReturned):
                return "Server returned from its listening thread.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kClientSetupFailed):
                return "Gateway client could not be setup.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kFailedToProvideService):
                return "Failed to provide service.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kConnectionFailure):
                return "Transport connection failed.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kNotConnected):
                return "Transport not connected.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kTimeout):
                return "Transport operation timed out.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kInvalidMessage):
                return "Invalid transport message.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kNotSupported):
                return "Operation not supported by this transport.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kSendFailure):
                return "Failed to send message.";
            case static_cast<score::result::ErrorCode>(TransportErrorc::kReceiveFailure):
                return "Failed to receive message.";
            default:
                return "unknown transport error";
        }
    }
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_TRANSPORT_ERROR_H
