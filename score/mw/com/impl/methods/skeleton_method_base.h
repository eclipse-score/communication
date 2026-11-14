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

#include "score/mw/com/impl/enable_reference_to_moveable_from_this.h"
#include "score/mw/com/impl/flag_owner.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"

#include <memory>

namespace score::mw::com::impl
{

class SkeletonMethodBaseView;

class SkeletonMethodBase : public EnableReferenceToMoveableFromThis<SkeletonMethodBase>
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a read only view to the private members of this class inside the impl
    // module.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend SkeletonMethodBaseView;

  public:
    SkeletonMethodBase(const std::string_view method_name,
                       std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding,
                       MethodType method_type)
        : EnableReferenceToMoveableFromThis<SkeletonMethodBase>(),
          method_name_{method_name},
          method_type_{method_type},
          binding_{std::move(skeleton_method_binding)}
    {
    }

    ~SkeletonMethodBase() override = default;

    SkeletonMethodBase(const SkeletonMethodBase&) = delete;
    SkeletonMethodBase& operator=(const SkeletonMethodBase&) & = delete;

    SkeletonMethodBase(SkeletonMethodBase&&) noexcept = default;
    SkeletonMethodBase& operator=(SkeletonMethodBase&&) & noexcept = default;

    /// \brief Notifies this method whether the enclosing Skeleton instance is currently offered or not.
    /// \details Called by SkeletonBase whenever its offer-state changes (i.e. on a successful OfferService() and on
    /// StopOfferService()). While the enclosing Skeleton instance is offered, RegisterHandler() shall be rejected in
    /// order to avoid races between an in-flight call being dispatched to an "old" handler and a concurrently
    /// re-registered "new" handler.
    void SetServiceOffered(const bool is_offered) noexcept
    {
        if (is_offered)
        {
            service_offered_flag_.Set();
        }
        else
        {
            service_offered_flag_.Clear();
        }
    }

    /// \brief Returns whether the enclosing Skeleton instance is currently offered.
    bool IsServiceOffered() const noexcept
    {
        return service_offered_flag_.IsSet();
    }

  protected:
    std::string_view method_name_;
    MethodType method_type_;
    std::unique_ptr<SkeletonMethodBinding> binding_;
    FlagOwner service_offered_flag_;
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
