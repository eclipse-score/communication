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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_KNOWN_INSTANCES_CONTAINER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_KNOWN_INSTANCES_CONTAINER_H

#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/handle_type.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace score::mw::com::impl::lola
{

/**
 * Container for keeping track of what instances of services for the LoLa binding are available.
 */
class KnownInstancesContainer final
{
  public:
    auto Insert(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept -> bool;
    auto Remove(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept -> void;
    auto GetKnownHandles(const EnrichedInstanceIdentifier& enriched_instance_identifier) const noexcept
        -> std::vector<HandleType>;

    auto Merge(KnownInstancesContainer&& container_to_be_merged) noexcept -> void;

    auto Empty() const noexcept -> bool;

  private:
    std::unordered_map<LolaServiceId, std::unordered_set<LolaServiceInstanceId::InstanceId>> known_instances_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_KNOWN_INSTANCES_CONTAINER_H
