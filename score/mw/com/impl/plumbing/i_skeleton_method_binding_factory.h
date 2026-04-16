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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_METHOD_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_METHOD_BINDING_FACTORY_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/method_size_info.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/skeleton_base.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Interface for a factory class that dispatches calls to a real or mock factory implementation based on binding
/// information in the deployment configuration.
class ISkeletonMethodBindingFactory
{
  public:
    ISkeletonMethodBindingFactory() = default;

    virtual ~ISkeletonMethodBindingFactory() = default;

    ISkeletonMethodBindingFactory(ISkeletonMethodBindingFactory&&) = delete;
    ISkeletonMethodBindingFactory& operator=(ISkeletonMethodBindingFactory&&) = delete;
    ISkeletonMethodBindingFactory(const ISkeletonMethodBindingFactory&) = delete;
    ISkeletonMethodBindingFactory& operator=(const ISkeletonMethodBindingFactory&) = delete;

    /// Creates an instance of the binding for a Skeleton method.
    /// size_info carries the method's in-args / return / queue size so the binding can publish it
    /// up-front (e.g. into shared memory for a generic proxy to read). Bindings that don't need
    /// that information are free to ignore it.
    virtual auto Create(const InstanceIdentifier& instance_identifier,
                        SkeletonBinding* parent_binding,
                        const std::string_view method_name,
                        const MethodSizeInfo& size_info) -> std::unique_ptr<SkeletonMethodBinding> = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_METHOD_BINDING_FACTORY_H
