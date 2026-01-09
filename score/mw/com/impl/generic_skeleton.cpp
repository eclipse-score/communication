/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/size_info.h"
#include "score/mw/com/impl/skeleton_binding.h"

namespace score::mw::com::impl
{

Result<GenericSkeleton> GenericSkeleton::Create(
    const InstanceSpecifier& specifier,
    MethodCallProcessingMode mode) noexcept
{
    const auto instance_identifier_result = GetInstanceIdentifier(specifier);

    if (!instance_identifier_result.has_value())
    {
        score::mw::log::LogFatal("lola") << "Failed to resolve instance identifier from instance specifier";
        std::terminate();
    }

    return Create(instance_identifier_result.value(),mode);
}

Result<GenericSkeleton> GenericSkeleton::Create(
    const InstanceIdentifier& identifier,
    MethodCallProcessingMode mode) noexcept
{
    auto binding = SkeletonBindingFactory::Create(identifier);
    if (!binding)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return GenericSkeleton(identifier, std::move(binding),mode);
}

Result<GenericSkeletonEvent*> GenericSkeleton::AddEvent(std::string_view name, const SizeInfo& size_info) noexcept
{
    auto skeleton_view = SkeletonBaseView{*this};
    if (skeleton_view.IsOffered())
    {
        // It is not allowed to add events after the service has been offered.
        return MakeUnexpected(ComErrc::kServiceInstanceAlreadyOffered);
    }

    auto event_binding_result = GenericSkeletonEventBindingFactory::Create(*this, name, size_info);
    if (!event_binding_result.has_value())
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    auto emplace_result = owned_events_.emplace(
        name, std::make_unique<GenericSkeletonEvent>(*this, name, std::move(event_binding_result).value()));
    if (!emplace_result.second)
    {
        // An event with this name has already been added.
        return MakeUnexpected(ComErrc::kServiceElementAlreadyExists);
    }

    return emplace_result.first->second.get();
}

Result<score::Blank> GenericSkeleton::OfferService() noexcept
{
    return SkeletonBase::OfferService();
}

void GenericSkeleton::StopOfferService() noexcept
{
    SkeletonBase::StopOfferService();
}

GenericSkeleton::GenericSkeleton(const InstanceIdentifier& identifier,
                                 std::unique_ptr<SkeletonBinding> binding,
                                 MethodCallProcessingMode mode)
    : SkeletonBase(std::move(binding), identifier,mode)
{
}

} // namespace score::mw::com::impl