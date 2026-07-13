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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_BINDING_FACTORY_ERROR_H
#define SCORE_MW_COM_IMPL_PLUMBING_BINDING_FACTORY_ERROR_H

#include "score/result/result.h"

#include <string_view>

namespace score::mw::com::impl
{

enum class BindingFactoryErrorCode : score::result::ErrorCode
{
    kInvalid,
    kParentBindingIsNotLola,
    kUnsupportedBindingType,
    kProxyCreationFailed,
    kNumEnumElements
};

class BindingFactoryErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    {
        switch (code)
        {
            case static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kParentBindingIsNotLola):
                return "Parent proxy binding is not a LoLa binding.";
            case static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kUnsupportedBindingType):
                return "Service type deployment contains an unsupported binding type.";
            case static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kProxyCreationFailed):
                return "Proxy binding creation failed.";
            case static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kInvalid):
            case static_cast<score::result::ErrorCode>(BindingFactoryErrorCode::kNumEnumElements):
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                    false,
                    "kNumEnumElements/kInvalid are not valid states for the enum! They're just used "
                    "for verifying the value of an enum during serialization / deserialization!");
            default:
                return "unknown binding factory error";
        }
    }
};

inline constexpr BindingFactoryErrorDomain kBindingFactoryErrorDomain{};

inline score::result::Error MakeError(const BindingFactoryErrorCode code, const std::string_view message = "")
{
    return {static_cast<score::result::ErrorCode>(code), kBindingFactoryErrorDomain, message};
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_BINDING_FACTORY_ERROR_H
