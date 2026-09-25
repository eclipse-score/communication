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
#include "score/mw/com/impl/bindings/someip/skeleton.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <utility>

namespace score::mw::com::impl::someip
{

std::unique_ptr<Skeleton> Skeleton::Create(const InstanceIdentifier& identifier, ITransport& transport) noexcept
{
    return std::make_unique<Skeleton>(identifier, transport);
}

Skeleton::Skeleton(const InstanceIdentifier& identifier, ITransport& transport) noexcept
    : SkeletonBinding{},
      identifier_{identifier},
      transport_{transport},
      quality_type_{InstanceIdentifierView{identifier_}.GetServiceInstanceDeployment().asilLevel_},
      event_data_storages_{}
{
}

Skeleton::~Skeleton() noexcept = default;

auto Skeleton::PrepareOffer(SkeletonEventBindings& events,
                            SkeletonFieldBindings& fields,
                            std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback)
    -> Result<void>
{
    // The SOME/IP binding does not use shared memory, so it must not call the shm-object trace callback. See the
    // contract documented on SkeletonBinding::PrepareOffer().
    score::cpp::ignore = register_shm_object_trace_callback;

    // The individual service elements offer themselves on the transport during their own PrepareOffer(). The skeleton
    // only has to make sure that it does not hold on to storage of a previous offering.
    score::cpp::ignore = events;
    score::cpp::ignore = fields;
    return {};
}

void Skeleton::PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback> unregister_shm_object_trace_callback)
{
    // See PrepareOffer(): the SOME/IP binding does not use shared memory.
    score::cpp::ignore = unregister_shm_object_trace_callback;
}

auto Skeleton::Register(const ElementFqId element_fq_id,
                        const SlotIndexType number_of_slots,
                        const memory::DataTypeSizeInfo data_type_size_info,
                        const std::optional<InitializeSampleCallback>& initialize_sample_callback) -> RegistrationResult
{
    const auto existing_storage_it = event_data_storages_.find(element_fq_id);
    if (existing_storage_it != event_data_storages_.end())
    {
        // The storage of a previous offering is reused. The initialize_sample_callback is deliberately ignored here:
        // re-initializing the slots would tamper with event data which a consumer of the previous offering could
        // still be reading. This mirrors lola::Skeleton::Register().
        return RegistrationResult{*existing_storage_it->second};
    }

    auto event_data_storage = std::make_unique<EventDataStorage>(number_of_slots, data_type_size_info);
    if (initialize_sample_callback.has_value())
    {
        event_data_storage->InitializeSlots(initialize_sample_callback.value());
    }

    const auto insert_result = event_data_storages_.emplace(element_fq_id, std::move(event_data_storage));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(insert_result.second, "Could not register event data storage.");
    return RegistrationResult{*insert_result.first->second};
}

}  // namespace score::mw::com::impl::someip
