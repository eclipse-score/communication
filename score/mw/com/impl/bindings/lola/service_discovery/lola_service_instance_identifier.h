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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_LOLA_SERVICE_INSTANCE_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_LOLA_SERVICE_INSTANCE_IDENTIFIER_H

#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

class LolaServiceInstanceIdentifier
{
  public:
    explicit LolaServiceInstanceIdentifier(LolaServiceId service_id) noexcept;
    explicit LolaServiceInstanceIdentifier(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept;

    LolaServiceId GetServiceId() const noexcept;
    std::optional<LolaServiceInstanceId::InstanceId> GetInstanceId() const noexcept;

  private:
    LolaServiceId service_id_;
    std::optional<LolaServiceInstanceId::InstanceId> instance_id_;
};

bool operator==(const LolaServiceInstanceIdentifier& lhs, const LolaServiceInstanceIdentifier& rhs) noexcept;

}  // namespace score::mw::com::impl::lola

namespace std
{
template <>
class hash<score::mw::com::impl::lola::LolaServiceInstanceIdentifier>
{
  public:
    std::size_t operator()(const score::mw::com::impl::lola::LolaServiceInstanceIdentifier& identifier) const noexcept;
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_LOLA_SERVICE_INSTANCE_IDENTIFIER_H
