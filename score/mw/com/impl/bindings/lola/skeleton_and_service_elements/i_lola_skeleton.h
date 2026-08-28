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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_AND_SERVICE_ELEMENTS_I_LOLA_SKELETON_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_AND_SERVICE_ELEMENTS_I_LOLA_SKELETON_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/methods/unique_method_identifier.h"
#include "score/mw/com/impl/bindings/lola/skeleton_and_service_elements/i_lola_skeleton_method.h"
#include "score/mw/com/impl/bindings/lola/skeleton_and_service_elements/skeleton_event_properties.h"

namespace score::mw::com::impl::lola
{

class ILolaSkeleton
{
  public:
    struct GenericRegistrationResult
    {
        void* type_erased_event_data_storage_ptr;
        EventControl& event_control_qm;
        EventControl* event_control_asil_b;
    };

    ILolaSkeleton() = default;
    virtual ~ILolaSkeleton() = default;

    ILolaSkeleton(const ILolaSkeleton&) = delete;
    ILolaSkeleton& operator=(const ILolaSkeleton&) & = delete;
    ILolaSkeleton(ILolaSkeleton&&) noexcept = delete;
    ILolaSkeleton& operator=(ILolaSkeleton&&) & noexcept = delete;

    /// \brief Enables dynamic registration of Generic (type-erased) Events at the Skeleton.
    /// \param element_fq_id The full qualified ID of the element (event) that shall be registered.
    /// \param element_properties Properties of the element (e.g. number of slots, max subscribers).
    /// \param sample_size The size of a single data sample in bytes.
    /// \param sample_alignment The alignment requirement of the data sample in bytes.
    /// \return A pair containing:
    ///         - An type erased pointer to the allocated data storage (void*).
    ///         - The EventDataControlComposite for managing the event's control data.
    virtual GenericRegistrationResult RegisterGeneric(const ElementFqId element_fq_id,
                                                      const SkeletonEventProperties& element_properties,
                                                      const size_t sample_size,
                                                      const size_t sample_alignment) = 0;

    virtual void RegisterMethod(const UniqueMethodIdentifier method_id, ILolaSkeletonMethod& skeleton_method) = 0;

    virtual QualityType GetInstanceQualityType() const = 0;

    virtual void DisconnectQmConsumers() = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_AND_SERVICE_ELEMENTS_I_LOLA_SKELETON_H
