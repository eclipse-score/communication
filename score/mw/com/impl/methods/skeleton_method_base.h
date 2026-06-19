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

#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/mw/com/impl/reference_to_moveable.h"

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
    SkeletonMethodBase(SkeletonBase&,
                       const std::string_view method_name,
                       std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding,
                       MethodType method_type = MethodType::kMethod)
        : method_name_{method_name},
          method_type_{method_type},
          binding_{std::move(skeleton_method_binding)},
          reference_to_moveable_{*this}
    {
    }

    ~SkeletonMethodBase() = default;

    SkeletonMethodBase(const SkeletonMethodBase&) = delete;
    SkeletonMethodBase& operator=(const SkeletonMethodBase&) & = delete;

    SkeletonMethodBase(SkeletonMethodBase&& other) noexcept
        : method_name_{other.method_name_},
          method_type_{other.method_type_},
          binding_{std::move(other.binding_)},
          reference_to_moveable_{std::move(other.reference_to_moveable_)}
    {
        reference_to_moveable_.Update(*this);
    }

    SkeletonMethodBase& operator=(SkeletonMethodBase&& other) & noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        method_name_ = other.method_name_;
        method_type_ = other.method_type_;
        binding_ = std::move(other.binding_);
        reference_to_moveable_ = std::move(other.reference_to_moveable_);

        reference_to_moveable_.Update(*this);
        return *this;
    }

  protected:
    std::string_view method_name_;
    MethodType method_type_;
    std::unique_ptr<SkeletonMethodBinding> binding_;

    /// \brief Helper class for creating reference to this SkeletonMethodBase which is provided to SkeletonBase when
    /// registering this event.
    ///
    /// Contains a heap allocated reference to this SkeletonMethodBase which is updated in the move constructor and move
    /// assignment operator so that it's always valid, even after moving the SkeletonMethodBase.
    ReferenceToMoveable<SkeletonMethodBase> reference_to_moveable_;
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
