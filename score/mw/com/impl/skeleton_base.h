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

    [[nodiscard]] score::ResultBlank OfferServiceEvents() const noexcept;
    [[nodiscard]] score::ResultBlank OfferServiceFields() const noexcept;

    FlagOwner service_offered_flag_;
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
        const auto result = skeleton_base_.events_.emplace(event_name, event);
        const bool was_event_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_event_inserted, "Event cannot be registered as it already exists.");
    }

    void RegisterField(const std::string_view field_name, SkeletonFieldBase& field)
    {
        const auto result = skeleton_base_.fields_.emplace(field_name, field);
        const bool was_field_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_field_inserted, "Field cannot be registered as it already exists.");
    }

    void RegisterMethod(const std::string_view method_name, SkeletonMethodBase& method)
    {
        const auto result = skeleton_base_.methods_.emplace(method_name, method);
        const bool was_method_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_method_inserted, "Method cannot be registered as it already exists.");
    }

    // Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
    // called implicitly". std::terminate() is implicitly called from std::map::at in case the container doesn't
    // have the mapped element. This function is called by the move constructor and as we register the event name in
    // the events_ container during SkeletonEvent construction so we are sure that the event_name already exists, so
    // no way for throwing std::out_of_range which leds to std::terminate().
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    void UpdateEvent(const std::string_view event_name, SkeletonEventBase& event) noexcept
    {
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "If a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // This function is indirectly called by the move constructor and as we register the event name in the
        // events_ container during SkeletonEvent construction so we are sure the event name already exists, so no
        // way for throwing std::out_of_range which leds to std::terminate().
        // coverity[autosar_cpp14_a15_4_2_violation] : FALSE]
        skeleton_base_.events_.at(event_name) = event;
    }

    void UpdateField(const std::string_view field_name, SkeletonFieldBase& field) noexcept
    {
        auto field_name_it = skeleton_base_.fields_.find(field_name);
        if (field_name_it == skeleton_base_.fields_.cend())
        {
            score::mw::log::LogError("lola")
                << "SkeletonBaseView::UpdateField failed to update field because the requested field doesn't exist";
            std::terminate();
        }

        field_name_it->second = field;
    }

    void UpdateMethod(const std::string_view method_name, SkeletonMethodBase& method) noexcept
    {
        auto method_name_it = skeleton_base_.methods_.find(method_name);
        if (method_name_it == skeleton_base_.methods_.cend())
        {
            score::mw::log::LogError("lola")
                << "SkeletonBaseView::UpdateMethod failed to update method because the requested method doesn't exist";
            std::terminate();
        }
        method_name_it->second = method;
    }

    const SkeletonBase::SkeletonEvents& GetEvents() const noexcept
    {
        return skeleton_base_.events_;
    }

    const SkeletonBase::SkeletonFields& GetFields() const noexcept
    {
        return skeleton_base_.fields_;
    }

    const SkeletonBase::SkeletonMethods& GetMethods() const noexcept
    {
        return skeleton_base_.methods_;
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