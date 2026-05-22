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
#ifndef SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_ERROR_H
#define SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_ERROR_H

#include "score/result/result.h"

#include <string_view>

namespace score::mw::com::gateway
{

enum class GatewayErrorc : score::result::ErrorCode
{
    kSkeletonCreationFailed = 1,
    kSkeletonOfferFailed,
    kSkeletonStopOfferFailed,
    kUnknownServiceInstance,
    kUnknownServiceElement,
    kReceiveHandlerRegistrationFailed,
    kNonWhitelistedService,
};

score::result::Error MakeError(const GatewayErrorc code, const std::string_view message = "");

class GatewayErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    {
        switch (code)
        {
            case static_cast<score::result::ErrorCode>(GatewayErrorc::kSkeletonCreationFailed):
                return "Creation of generic skeleton on destination gateway failed.";
            case static_cast<score::result::ErrorCode>(GatewayErrorc::kSkeletonOfferFailed):
                return "Offering of generic skeleton on destination gateway failed.";
            case static_cast<score::result::ErrorCode>(GatewayErrorc::kUnknownServiceInstance):
                return "Gateway received an API call for an unknown service instance identifier.";
            case static_cast<score::result::ErrorCode>(GatewayErrorc::kUnknownServiceElement):
                return "Gateway received an API call for an unknown service element within an instance identifier.";
            case static_cast<score::result::ErrorCode>(GatewayErrorc::kReceiveHandlerRegistrationFailed):
                return "Gateway couldn't register event-receive-handler at its generic proxy to forward event update "
                       "notifications.";
            case static_cast<score::result::ErrorCode>(GatewayErrorc::kNonWhitelistedService):
                return "Gateway received request to provide a non-whitelised service.";
            default:
                return "unknown gateway error";
        }
    }
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_GATEWAY_APPLICATION_GATEWAY_ERROR_H
