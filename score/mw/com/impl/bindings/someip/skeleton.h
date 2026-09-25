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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/someip/element_fq_id.h"
#include "score/mw/com/impl/bindings/someip/event_data_storage.h"
#include "score/mw/com/impl/bindings/someip/i_transport.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/initialize_sample_callback.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace score::mw::com::impl::someip
{

/// \brief Skeleton binding of the SOME/IP binding.
///
/// \details Mirrors lola::Skeleton in role and API shape, but is far smaller: LoLa has to create, open, size and roll
///          back shared-memory regions, whereas the SOME/IP binding only has to offer/stop-offer its service elements
///          on the transport and own the process-local slot storage of its events.
class Skeleton final : public SkeletonBinding
{
  public:
    /// \brief Result of registering a service element at the Skeleton.
    class RegistrationResult
    {
      public:
        // coverity[autosar_cpp14_m11_0_1_violation]
        EventDataStorage& event_data_storage;
    };

    static std::unique_ptr<Skeleton> Create(const InstanceIdentifier& identifier, ITransport& transport) noexcept;

    Skeleton(const InstanceIdentifier& identifier, ITransport& transport) noexcept;

    ~Skeleton() noexcept override;

    Skeleton(const Skeleton&) = delete;
    Skeleton(Skeleton&&) noexcept = delete;
    Skeleton& operator=(const Skeleton&) & = delete;
    Skeleton& operator=(Skeleton&&) & noexcept = delete;

    Result<void> PrepareOffer(
        SkeletonEventBindings& events,
        SkeletonFieldBindings& fields,
        std::optional<RegisterShmObjectTraceCallback> register_shm_object_trace_callback) override;

    void PrepareStopOffer(
        std::optional<UnregisterShmObjectTraceCallback> unregister_shm_object_trace_callback) override;

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }

    /// \brief The SOME/IP binding does not support methods yet, so there are never unregistered method handlers.
    bool VerifyAllMethodHandlersRegistered() const override
    {
        return true;
    }

    /// \brief Enables dynamic registration of events (typed and generic) at the Skeleton.
    /// \param element_fq_id The fully qualified ID of the element (event) that shall be registered.
    /// \param number_of_slots Number of sample slots the element needs.
    /// \param data_type_size_info The size and alignment of a single data sample in bytes.
    /// \param initialize_sample_callback Optional callback to initialize a sample in the underlying type-erased
    ///        storage slots. Only used in case the EventDataStorage is freshly created in this call. If an already
    ///        existing EventDataStorage is returned, the callback is ignored, since re-initializing the slots would
    ///        potentially tamper with event data which is still in use.
    /// \return The registered data structures within the Skeleton.
    RegistrationResult Register(const ElementFqId element_fq_id,
                                const SlotIndexType number_of_slots,
                                const memory::DataTypeSizeInfo data_type_size_info,
                                const std::optional<InitializeSampleCallback>& initialize_sample_callback);

    QualityType GetInstanceQualityType() const noexcept
    {
        return quality_type_;
    }

    ITransport& GetTransport() const noexcept
    {
        return transport_;
    }

  private:
    InstanceIdentifier identifier_;
    ITransport& transport_;
    QualityType quality_type_;
    std::unordered_map<ElementFqId, std::unique_ptr<EventDataStorage>> event_data_storages_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H
