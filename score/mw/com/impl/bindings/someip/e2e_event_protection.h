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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_E2E_EVENT_PROTECTION_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_E2E_EVENT_PROTECTION_H

#include "score/mw/com/impl/configuration/e2e_event_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/e2e/e2e_profile_stub.h"
#include "score/mw/com/impl/e2e/e2e_result.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

namespace score::mw::com::impl::someip
{

/// \brief A protected SOME/IP event frame is laid out as [E2E header | serialized sample].
constexpr std::size_t kE2EHeaderSize{e2e::kProfileStubHeaderSize};

/// \brief Protects the outgoing frames of one SOME/IP event. Owned by the skeleton event.
class E2EEventProtector final
{
  public:
    explicit E2EEventProtector(const E2EEventTypeDeployment& deployment) noexcept;

    /// \brief Writes the E2E header into the first kE2EHeaderSize bytes of the frame, covering the remaining bytes.
    /// \return False if the frame cannot hold the header. The frame is then left untouched.
    bool Protect(score::cpp::span<std::byte> frame) noexcept;

  private:
    e2e::ProfileConfiguration configuration_;
    e2e::ProtectContext context_;
};

/// \brief Checks the received frames of one SOME/IP event. Owned by the proxy event.
class E2EEventChecker final
{
  public:
    explicit E2EEventChecker(const E2EEventTypeDeployment& deployment) noexcept;

    /// \brief Checks one received frame. A frame too short for the header fails data integrity and skips the
    ///        sequence check.
    e2e::RawCheckOutcome Check(score::cpp::span<const std::byte> frame) noexcept;

  private:
    e2e::ProfileConfiguration configuration_;
    e2e::CheckContext context_;
};

/// \brief Creates the protector for the given event, or std::nullopt if E2E is not configured for it.
std::optional<E2EEventProtector> CreateE2EEventProtector(const SomeIpServiceTypeDeployment& type_deployment,
                                                         std::string_view event_name);

/// \brief Creates the checker for the given event, or std::nullopt if E2E is not configured for it.
std::optional<E2EEventChecker> CreateE2EEventChecker(const SomeIpServiceTypeDeployment& type_deployment,
                                                     std::string_view event_name);

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_E2E_EVENT_PROTECTION_H
