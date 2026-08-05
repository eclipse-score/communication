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

#ifndef SCORE_SOCOM_ERROR_HPP
#define SCORE_SOCOM_ERROR_HPP

#include <string_view>

#include "score/result/error_code.h"
#include "score/result/error_domain.h"
#include "score/result/result.h"

namespace score::socom {

/// \brief Error conditions when using Client_connector.
enum class Error : score::result::ErrorCode {
    /// Service state is not Service_state::available. Service_state::available cannot prevent
    /// network issues, so if it is important that the Server receives a method call, it always has
    /// to send some return value via the callback.
    runtime_error_service_not_available,
    /// Request is rejected.
    runtime_error_request_rejected,
    /// Event or method ID is out of range.
    logic_error_id_out_of_range,
    /// Payload cannot be deserialized.
    runtime_error_malformed_payload,
    /// Access is denied.
    runtime_error_permission_not_allowed,
};

score::result::Error MakeError(Error code, std::string_view user_message = "") noexcept;

/// \brief Error conditions when using Enabled_server_connector.
enum class Server_connector_error : score::result::ErrorCode {
    /// Event or method ID is out of range.
    logic_error_id_out_of_range,
    runtime_error_no_client_subscribed_for_event,
};

score::result::Error MakeError(Server_connector_error code,
                               std::string_view user_message = "") noexcept;

/// \brief Errors upon connector construction.
enum class Construction_error : score::result::ErrorCode {
    duplicate_service,  ///< Service identifier already exists.
    duplicate_client,   ///< Client already exists.
    callback_missing    ///< At least one of the provided callbacks is missing.
};

score::result::Error MakeError(Construction_error code,
                               std::string_view user_message = "") noexcept;

}  // namespace score::socom

#endif  // SCORE_SOCOM_ERROR_HPP
