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
#ifndef SCORE_MW_COM_IMPL_SKELETON_BASE_H
#define SCORE_MW_COM_IMPL_SKELETON_BASE_H

#include "score/mw/com/impl/flag_owner.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/skeleton_service_elements.h"

#include "score/mw/com/impl/mocking/i_skeleton_base.h"

#include "score/result/result.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/span.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

class SkeletonEventBase;
class SkeletonFieldBase;
class SkeletonMethodBase;

/// \brief Parent class for all generated skeletons. Only the generated skeletons will be user facing. But in order to
/// reduce code duplication, we encapsulate the common logic in here.
class SkeletonBase
{
  public:
    // Re-export the element-map type aliases so that the rest of the codebase can
    // continue to use SkeletonBase::SkeletonEvents etc. without change.
    using SkeletonEvents = SkeletonServiceElements::SkeletonEvents;
    using SkeletonFields = SkeletonServiceElements::SkeletonFields;
    using SkeletonMethods = SkeletonServiceElements::SkeletonMethods;

    /// \brief Creation of service skeleton with provided Skeleton binding
    ///
    /// \requirement SWS_CM_00130
    ///
    /// \param skeleton_binding The SkeletonBinding which is created using SkeletonBindingFactory.
    /// \param instance_id The instance identifier which uniquely identifies this Skeleton instance.
    SkeletonBase(std::unique_ptr<SkeletonBinding> skeleton_binding, InstanceIdentifier instance_id);

    virtual ~SkeletonBase() = default;

    /// \brief A Skeleton shall not be copyable
    /// \requirement SWS_CM_00134
    SkeletonBase(const SkeletonBase&) = delete;
    SkeletonBase& operator=(const SkeletonBase&) = delete;

    /**
     * \api
     * \brief Offer the respective service to other applications
     * \return On failure, returns an error code according to the SW Component Requirements SCR-17434118 and
     * SCR-566325.
     */
    [[nodiscard]] ResultBlank OfferService() noexcept;

    /**
     * \api
     * \brief Stops offering the respective service to other applications
     * \requirement SWS_CM_00111
     */
    void StopOfferService() noexcept;

    void InjectMock(ISkeletonBase& skeleton_mock)
    {
        skeleton_mock_ = &skeleton_mock;
    }

  protected:
    bool AreBindingsValid() const noexcept;

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonBaseView;

    /// \brief A Skeleton shall be movable.
    ///
    /// \details The move constructor and move assignment operator cannot be defaulted because,
    ///          after moving all data members, they must call
    ///          service_elements_.UpdateSkeletonReferences(*this) so that every registered event,
    ///          field, and method updates its back-pointer to the new SkeletonBase address.
    ///          The per-element iteration logic lives in SkeletonServiceElements, keeping these
    ///          implementations intentionally short and free of duplicated loops.
    ///
    /// \requirement SWS_CM_00135
    SkeletonBase(SkeletonBase&& other) noexcept;
    SkeletonBase& operator=(SkeletonBase&& other) noexcept;

  private:
    std::unique_ptr<SkeletonBinding> binding_;

    /// Holds the maps of registered events, fields, and methods together with the
    /// post-move reference-update logic (see SkeletonServiceElements).
    SkeletonServiceElements service_elements_;

    InstanceIdentifier instance_id_;
    ISkeletonBase* skeleton_mock_;
    FlagOwner service_offered_flag_;

    [[nodiscard]] score::ResultBlank OfferServiceEvents() const noexcept;
    [[nodiscard]] score::ResultBlank OfferServiceFields() const noexcept;

    /// \brief Called at the end of both the move constructor and the move assignment operator.
    /// Iterates all registered events, fields, and methods and calls UpdateSkeletonReference(*this)
    /// so that each element's back-pointer is updated to the new SkeletonBase address.
    /// This method exists in SkeletonBase (rather than SkeletonServiceElements) because it needs
    /// the complete type of SkeletonBase, which would create a circular dependency if placed
    /// in skeleton_service_elements.cpp.
    void UpdateAllServiceElementReferences() noexcept;
};

class SkeletonBaseView
{
  public:
    explicit SkeletonBaseView(SkeletonBase& skeleton_base) : skeleton_base_{skeleton_base} {}

    InstanceIdentifier GetAssociatedInstanceIdentifier() const
    {
        return skeleton_base_.instance_id_;
    }

    SkeletonBinding* GetBinding() const
    {
        return skeleton_base_.binding_.get();
    }

    void RegisterEvent(const std::string_view event_name, SkeletonEventBase& event)
    {
        skeleton_base_.service_elements_.RegisterEvent(event_name, event);
    }

    void RegisterField(const std::string_view field_name, SkeletonFieldBase& field)
    {
        skeleton_base_.service_elements_.RegisterField(field_name, field);
    }

    void RegisterMethod(const std::string_view method_name, SkeletonMethodBase& method)
    {
        skeleton_base_.service_elements_.RegisterMethod(method_name, method);
    }

    void UpdateEvent(const std::string_view event_name, SkeletonEventBase& event) noexcept
    {
        skeleton_base_.service_elements_.UpdateEvent(event_name, event);
    }

    void UpdateField(const std::string_view field_name, SkeletonFieldBase& field) noexcept
    {
        skeleton_base_.service_elements_.UpdateField(field_name, field);
    }

    void UpdateMethod(const std::string_view method_name, SkeletonMethodBase& method) noexcept
    {
        skeleton_base_.service_elements_.UpdateMethod(method_name, method);
    }

    const SkeletonBase::SkeletonEvents& GetEvents() const noexcept
    {
        return skeleton_base_.service_elements_.GetEvents();
    }

    const SkeletonBase::SkeletonFields& GetFields() const noexcept
    {
        return skeleton_base_.service_elements_.GetFields();
    }

    const SkeletonBase::SkeletonMethods& GetMethods() const noexcept
    {
        return skeleton_base_.service_elements_.GetMethods();
    }

    bool AreBindingsValid() const
    {
        return skeleton_base_.AreBindingsValid();
    }

  private:
    SkeletonBase& skeleton_base_;
};

score::cpp::optional<InstanceIdentifier> GetInstanceIdentifier(const InstanceSpecifier& specifier);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_BASE_H
