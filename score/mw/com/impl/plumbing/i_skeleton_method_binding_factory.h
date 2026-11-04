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
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/skeleton_base.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <optional>
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

    /// Creates instances of the method binding of a Skeleton method with a particular data type.
    /// \param instance_identifier The instance identifier containing the binding information.
    /// \param parent_binding A reference to the Skeleton which owns this method.
    /// \param method_name The binding unspecific name of the  inside the skeleton denoted by instance identifier.
    /// \param in_arg_type_info Size/alignment info of the method's in-args, as determined by the caller (which knows
    /// the concrete argument types), or std::nullopt if the method has no in-args.
    /// \param return_type_info Size/alignment info of the method's return type, as determined by the caller (which
    /// knows the concrete return type), or std::nullopt if the method has a void return type.
    /// \return An instance of SkeletonMethodBinding or nullptr in case of an error.
    virtual auto Create(const InstanceIdentifier& instance_identifier,
                        SkeletonBinding& parent_binding,
                        const std::string_view method_name,
                        MethodType method_type,
                        std::optional<memory::DataTypeSizeInfo> in_arg_type_info,
                        std::optional<memory::DataTypeSizeInfo> return_type_info)
        -> std::unique_ptr<SkeletonMethodBinding> = 0;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_I_SKELETON_METHOD_BINDING_FACTORY_H
