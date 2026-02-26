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
#ifndef SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_BASE_H
#define SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_BASE_H

#include "score/mw/com/impl/methods/skeleton_method_binding.h"

#include <functional>
#include <memory>

namespace score::mw::com::impl
{

class SkeletonMethodBaseView;

// forward declaration to avoid cyclical dependencies
class SkeletonBase;

class SkeletonMethodBase
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a read only view to the private members of this class inside the impl
    // module.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend SkeletonMethodBaseView;

  public:
    SkeletonMethodBase(SkeletonBase& skeleton_base,
                       const std::string_view method_name,
                       std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding)
        : method_name_{method_name}, binding_{std::move(skeleton_method_binding)}, skeleton_base_{skeleton_base}
    {
    }

    ~SkeletonMethodBase() = default;

    SkeletonMethodBase(const SkeletonMethodBase&) = delete;
    SkeletonMethodBase& operator=(const SkeletonMethodBase&) & = delete;

    SkeletonMethodBase(SkeletonMethodBase&&) noexcept = default;
    SkeletonMethodBase& operator=(SkeletonMethodBase&&) & noexcept = default;

    void UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept;

  protected:
    std::string_view method_name_;
    std::unique_ptr<SkeletonMethodBinding> binding_;
    std::reference_wrapper<SkeletonBase> skeleton_base_;
};

class SkeletonMethodBaseView
{
  public:
    explicit SkeletonMethodBaseView(SkeletonMethodBase& skeleton_method_base)
        : skeleton_method_base_{skeleton_method_base}
    {
    }

    SkeletonMethodBinding* GetMethodBinding()
    {
        return skeleton_method_base_.binding_.get();
    }

  private:
    SkeletonMethodBase& skeleton_method_base_;
};
}  // namespace score::mw::com::impl
#endif  // SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_BASE_H
