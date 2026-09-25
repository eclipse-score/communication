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
#ifndef SCORE_MW_COM_IMPL_E2E_E2E_PROFILE_STUB_H
#define SCORE_MW_COM_IMPL_E2E_E2E_PROFILE_STUB_H

#include "score/mw/com/impl/e2e/e2e_result.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::e2e
{

/// \brief Static profile inputs required by the POC stub.
struct ProfileConfiguration
{
    std::uint8_t data_id;
    std::uint8_t max_delta_counter;
};

/// \brief Sender-local state. One instance belongs to one protected event stream.
struct ProtectContext
{
    std::uint8_t next_counter{0U};
};

/// \brief Receiver-local state. One instance belongs to one consuming event endpoint.
struct CheckContext
{
    std::uint8_t last_counter{0U};
    bool has_last_counter{false};
};

/// \brief Categorised outcome produced for one checked POC frame.
struct CheckOutcome
{
    DataIntegrityStatus data_integrity;
    SequenceStatus sequence;
};

/// \brief Fixed header size used only by this POC profile stub.
constexpr std::size_t kProfileStubHeaderSize{4U};

/// \brief Adds deterministic POC metadata to a fixed-size header.
/// \details The header contains a data ID, an eight-bit counter,
///          and a checksum calculated over those fields and the protected payload.
/// \return False when header does not have exactly kProfileStubHeaderSize bytes.
bool ProtectMessage(score::cpp::span<std::byte> header,
                    score::cpp::span<const std::byte> payload,
                    ProfileConfiguration configuration,
                    ProtectContext& protect_context) noexcept;

/// \brief Verifies one POC header and updates the receiver-local sequence state for valid headers.
/// \details An invalid header or checksum produces kError and does not update check_context.
CheckOutcome CheckMessage(score::cpp::span<const std::byte> header,
                          score::cpp::span<const std::byte> payload,
                          ProfileConfiguration configuration,
                          CheckContext& check_context) noexcept;

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_PROFILE_STUB_H
