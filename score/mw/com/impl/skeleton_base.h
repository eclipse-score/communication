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

#include "score/mw/com/impl/mocking/i_skeleton_base.h"

#include "score/result/result.h"

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
    using SkeletonEvents = std::map<std::string_view, std::reference_wrapper<SkeletonEventBase>>;
    using SkeletonFields = std::map<std::string_view, std::reference_wrapper<SkeletonFieldBase>>;
    using SkeletonMethods = std::map<std::string_view, std::reference_wrapper<SkeletonMethodBase>>;

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
    [[nodiscard]] Result<void> OfferService() noexcept;

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
    [[nodiscard]] bool AreBindingsValid() const noexcept;

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonBaseView;

    /// \brief A Skeleton shall be movable
    /// \requirement SWS_CM_00135
    SkeletonBase(SkeletonBase&& other) noexcept;
    SkeletonBase& operator=(SkeletonBase&& other) noexcept;

  private:
    std::unique_ptr<SkeletonBinding> binding_;
    SkeletonEvents events_;
    SkeletonFields fields_;
    SkeletonMethods methods_;
    InstanceIdentifier instance_id_;

    ISkeletonBase* skeleton_mock_;

    [[nodiscard]] score::Result<void> OfferServiceEvents() const noexcept;
    [[nodiscard]] score::Result<void> OfferServiceFields() const noexcept;

    void UpdateAllServiceElementReferences() noexcept;

    FlagOwner service_offered_flag_;
};

class SkeletonBaseView
{
  public:
    explicit SkeletonBaseView(SkeletonBase& skeleton_base);

    [[nodiscard]] InstanceIdentifier GetAssociatedInstanceIdentifier() const;

    [[nodiscard]] SkeletonBinding* GetBinding() const;

    void RegisterEvent(std::string_view event_name, SkeletonEventBase& event);

    void RegisterField(std::string_view field_name, SkeletonFieldBase& field);

    void RegisterMethod(std::string_view method_name, SkeletonMethodBase& method);

    void UpdateEvent(std::string_view event_name, SkeletonEventBase& event) noexcept;

    void UpdateField(std::string_view field_name, SkeletonFieldBase& field) noexcept;

    void UpdateMethod(std::string_view method_name, SkeletonMethodBase& method) noexcept;

    [[nodiscard]] const SkeletonBase::SkeletonEvents& GetEvents() const noexcept;

    [[nodiscard]] const SkeletonBase::SkeletonFields& GetFields() const noexcept;

    [[nodiscard]] const SkeletonBase::SkeletonMethods& GetMethods() const noexcept;

    [[nodiscard]] bool AreBindingsValid() const;

  private:
    SkeletonBase& skeleton_base_;
};

score::cpp::optional<InstanceIdentifier> GetInstanceIdentifier(const InstanceSpecifier& specifier);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_BASE_H
