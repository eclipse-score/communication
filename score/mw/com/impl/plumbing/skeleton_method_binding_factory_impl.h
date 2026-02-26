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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_METHOD_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_METHOD_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/plumbing/i_skeleton_method_binding_factory.h"

#include <score/assert.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
class SkeletonMethodBindingFactoryImpl : public ISkeletonMethodBindingFactory
{
  public:
    std::unique_ptr<SkeletonMethodBinding> Create(const InstanceIdentifier& instance_identifier,
                                                  SkeletonBinding* parent_binding,
                                                  const std::string_view method_name) override;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_METHOD_BINDING_FACTORY_IMPL_H
