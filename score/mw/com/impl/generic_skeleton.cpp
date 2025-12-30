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
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/size_info.h"
#include "score/mw/com/impl/skeleton_binding.h"

namespace score::mw::com::impl
{

Result<GenericSkeleton> GenericSkeleton::Create(const InstanceSpecifier& specifier) noexcept
{
    const auto identifier_result = Runtime::getInstance().resolve(specifier);
    if (identifier_result.empty())
    {
        return MakeUnexpected(ComErrc::kInstanceIDCouldNotBeResolved);
    }

    if (identifier_result.size() > 1)
    {
        score::mw::log::LogWarn("com") << "InstanceSpecifier resolved to more than one InstanceIdentifier. Using the first one.";
    }

    return Create(identifier_result[0]);
}

Result<GenericSkeleton> GenericSkeleton::Create(const InstanceIdentifier& identifier) noexcept
{
    auto binding = SkeletonBindingFactory::Create(identifier);
    if (!binding)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return GenericSkeleton(identifier, std::move(binding));
}

Result<GenericSkeletonEvent*> GenericSkeleton::AddEvent(std::string_view name, const SizeInfo& size_info) noexcept
{
    if (SkeletonBaseView{*this}.IsOffered())
    {
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    auto event_binding_result =
        SkeletonBaseView{*this}.GetBinding()->CreateGenericEventBinding(name, size_info.size, size_info.alignment);
    if (!event_binding_result.has_value())
    {
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    auto emplace_result = events_.emplace(
        name, std::make_unique<GenericSkeletonEvent>(*this, name, std::move(event_binding_result).value()));
    if (!emplace_result.second)
    {
        return MakeUnexpected(ComErrc::kEventNotExisting);
    }
    auto& event = *emplace_result.first->second;
    SkeletonBaseView{*this}.RegisterEvent(name, event);

    return &event;
}

Result<score::Blank> GenericSkeleton::OfferService() noexcept
{
    return SkeletonBase::OfferService();
}

void GenericSkeleton::StopOfferService() noexcept
{
    SkeletonBase::StopOfferService();
}

GenericSkeleton::GenericSkeleton(const InstanceIdentifier& identifier, std::unique_ptr<SkeletonBinding> binding)
    : SkeletonBase(std::move(binding), identifier)
{
}

} // namespace score::mw::com::impl