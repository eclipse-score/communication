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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_FIELD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_FIELD_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <score/string_view.hpp>

#include <memory>

namespace score::mw::com::impl
{

/// \brief Interface for a factory class that dispatches calls to the appropriate binding based on
/// binding information in the deployment configuration.
template <typename SampleType>
class ISkeletonFieldBindingFactory
{
  public:
    ISkeletonFieldBindingFactory() noexcept = default;

    virtual ~ISkeletonFieldBindingFactory() noexcept = default;

    ISkeletonFieldBindingFactory(ISkeletonFieldBindingFactory&&) = delete;
    ISkeletonFieldBindingFactory& operator=(ISkeletonFieldBindingFactory&&) = delete;
    ISkeletonFieldBindingFactory(const ISkeletonFieldBindingFactory&) = delete;
    ISkeletonFieldBindingFactory& operator=(const ISkeletonFieldBindingFactory&) = delete;

    /// Creates instances of the event binding of a Skeleton field with a particular data type.
    /// \tparam SampleType Type of the data that is exchanged
    /// \param identifier The instance identifier containing the binding information.
    /// \param parent A reference to the Skeleton which owns this event.
    /// \param field_name The binding unspecific name of the field inside the skeleton denoted by instance identifier.
    /// \return An instance of SkeletonEventBinding or nullptr in case of an error.
    virtual auto CreateEventBinding(const InstanceIdentifier& identifier,
                                    SkeletonBase& parent,
                                    const score::cpp::string_view field_name) noexcept
        -> std::unique_ptr<SkeletonEventBinding<SampleType>> = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_FIELD_BINDING_FACTORY_H
