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

#include "score/socom/error.hpp"

namespace score::socom {
namespace {

class Error_error_domain final : public score::result::ErrorDomain {
   public:
    std::string_view MessageFor(score::result::ErrorCode const& code) const noexcept override {
        switch (static_cast<Error>(code)) {
            case Error::runtime_error_service_not_available:
                return "Service not available";
            case Error::runtime_error_request_rejected:
                return "Request rejected";
            case Error::logic_error_id_out_of_range:
                return "ID out of range";
            case Error::runtime_error_malformed_payload:
                return "Malformed payload";
            case Error::runtime_error_permission_not_allowed:
                return "Permission not allowed";
            default:
                return "Unknown Error";
        }
    }
};

class Server_connector_error_domain final : public score::result::ErrorDomain {
   public:
    std::string_view MessageFor(score::result::ErrorCode const& code) const noexcept override {
        switch (static_cast<Server_connector_error>(code)) {
            case Server_connector_error::logic_error_id_out_of_range:
                return "ID out of range";
            case Server_connector_error::runtime_error_no_client_subscribed_for_event:
                return "No client subscribed for event";
            default:
                return "Unknown Error";
        }
    }
};

class Construction_error_domain final : public score::result::ErrorDomain {
   public:
    std::string_view MessageFor(score::result::ErrorCode const& code) const noexcept override {
        switch (static_cast<Construction_error>(code)) {
            case Construction_error::duplicate_service:
                return "Duplicate service";
            case Construction_error::duplicate_client:
                return "Duplicate client";
            case Construction_error::callback_missing:
                return "Callback missing";
            default:
                return "Unknown Error";
        }
    }
};

}  // namespace

score::result::Error MakeError(Error code, std::string_view user_message) noexcept {
    static constexpr Error_error_domain error_domain;
    return {static_cast<score::result::ErrorCode>(code), error_domain, user_message};
}

score::result::Error MakeError(Server_connector_error code,
                               std::string_view user_message) noexcept {
    static constexpr Server_connector_error_domain error_domain;
    return {static_cast<score::result::ErrorCode>(code), error_domain, user_message};
}

score::result::Error MakeError(Construction_error code, std::string_view user_message) noexcept {
    static constexpr Construction_error_domain error_domain;
    return {static_cast<score::result::ErrorCode>(code), error_domain, user_message};
}

}  // namespace score::socom
