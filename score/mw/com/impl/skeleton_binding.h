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
#ifndef SCORE_MW_COM_IMPL_SKELETON_BINDING_H
#define SCORE_MW_COM_IMPL_SKELETON_BINDING_H

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/result/result.h"
#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/service_element_type.h"

#include <score/callback.hpp>
#include <score/optional.hpp>
#include <score/string_view.hpp>

#include <functional>
#include <map>

namespace score::mw::com::impl
{

class SkeletonEventBindingBase;

/**
 * \brief The SkeletonBinding abstracts the interface that _every_ binding needs to provide. It will be used by a
 * concrete skeleton to perform _any_ operation in a then binding specific manner.
 */
class SkeletonBinding
{
  public:
    using SkeletonEventBindings = std::map<score::cpp::string_view, std::reference_wrapper<SkeletonEventBindingBase>>;

    /// \brief callback type for registering shared-memory objects with tracing
    /// \details Needs only get used/called by bindings, which use shared-memory as their underlying communication/
    ///          data-exchange mechanism.
    using RegisterShmObjectTraceCallback =
        score::cpp::callback<void(score::cpp::string_view element_name,
                           ServiceElementType element_type,
                           memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd,
                           void* shm_memory_start_address),
                      64U>;

    /// \brief callback type for unregistering shared-memory objects with tracing
    /// \details Needs only get used/called by bindings, which use shared-memory as their underlying communication/
    ///          data-exchange mechanism.
    using UnregisterShmObjectTraceCallback =
        score::cpp::callback<void(score::cpp::string_view element_name, ServiceElementType element_type), 64U>;

    // For the moment, SkeletonFields use only SkeletonEventBindings. However, in the future when Get / Set are
    // supported in fields, then SkeletonFieldBindings will be a map in which the values are:
    // std::tuple<SkeletonEventBindingBase, SkeletonMethodBindingBase, SkeletonMethodBindingBase>
    using SkeletonFieldBindings = SkeletonEventBindings;

    SkeletonBinding() = default;

    // A SkeletonBinding is always held via a pointer in the binding independent impl::SkeletonBase.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::SkeletonBase.
    SkeletonBinding(const SkeletonBinding&) = delete;
    SkeletonBinding(SkeletonBinding&&) noexcept = delete;

    SkeletonBinding& operator=(const SkeletonBinding&) & = delete;
    SkeletonBinding& operator=(SkeletonBinding&&) & noexcept = delete;

    virtual ~SkeletonBinding();

    /// \brief In case there are any binding specifics with regards to service offerings, this can be implemented within
    ///  this function. It shall be called before actually offering the service over the service discovery mechanism.
    ///  A SkeletonBinding doesn't know its events so they should be passed "on-demand" into PrepareOffer() in case it
    ///  needs the events in order to complete the offering.
    ///  The optional RegisterShmObjectTraceCallback is handed over, in case tracing is enabled for elements within this
    ///  skeleton instance. If it is handed over AND the binding is using shared-memory as its underlying data-exchange
    ///  mechanism, it must call this callback for each shm-object it will use.
    ///
    /// \return expects void
    virtual ResultBlank PrepareOffer(SkeletonEventBindings&,
                                     SkeletonFieldBindings&,
                                     std::optional<RegisterShmObjectTraceCallback>) noexcept = 0;

    /**
     * \brief In case there are any binding specifics with regards to service withdrawals, this can be implemented
     * within this function. It shall be called before StopOffering() the service.
     *
     * \return void
     */
    virtual void PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback>) noexcept = 0;

    /// \brief Gets the binding type of the binding
    virtual BindingType GetBindingType() const noexcept = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_BINDING_H
