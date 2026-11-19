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

/********************************************************************************
 * io_dispatcher error domain, implemented with the score::result error-code /
 * error-domain customization point (ADL MakeError).
 ********************************************************************************/

#ifndef SCORE_IO_DISPATCHER_IO_DISPATCHER_ERROR_H
#define SCORE_IO_DISPATCHER_IO_DISPATCHER_ERROR_H

#include "score/result/error.h"
#include <string_view>

namespace io_dispatcher
{

/**
 * @brief Error codes reported by the io_dispatcher API.
 */
enum class IoDispatcherErrc : score::result::ErrorCode
{
    kApiError,      /*!< Invalid arguments / handle not registered / illegal state. */
    kInvalidHandle, /*!< The native handle an event source was registered for is no longer valid. */
    kResource,      /*!< A resource limit was reached or allocation failed. */
    kFatal          /*!< Unrecoverable error. */
};

/**
 * @brief Error domain translating IoDispatcherErrc codes to human-readable messages.
 */
class IoDispatcherErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override;
};

/**
 * @brief ADL customization point used by score::result::Error to build an Error from
 *        an IoDispatcherErrc. Enables `return score::MakeUnexpected(IoDispatcherErrc::k...)`.
 */
score::result::Error MakeError(const IoDispatcherErrc code, const std::string_view user_message = "") noexcept;

}  // namespace io_dispatcher

#endif  // SCORE_IO_DISPATCHER_IO_DISPATCHER_ERROR_H
