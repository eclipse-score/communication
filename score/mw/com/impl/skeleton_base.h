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
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include "score/memory/string_literal.h"
#include "score/result/result.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/optional.hpp>
#include <score/span.hpp>
#include <score/string_view.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace score::mw::com::impl
{

class SkeletonEventBase;
class SkeletonFieldBase;

/// \brief Defines the processing modes for the service implementation side.
///
/// \requirement SWS_CM_00301
///
enum class MethodCallProcessingMode : std::uint8_t
{
    kPoll,
    kEvent,
    kEventSingleThread
};

/// \brief Parent class for all generated skeletons. Only the generated skeletons will be user facing. But in order to
/// reduce code duplication, we encapsulate the common logic in here.
class SkeletonBase
{
  public:
    using EventNameList = score::cpp::span<const score::StringLiteral>;
    // An std::map/ordered map is NEEDED here as we require deterministic order of elements in the map, when iterating
    // over it repeatedly! A hint, that our shared-memory-size-calculation relies on it!
    using SkeletonEvents = std::map<score::cpp::string_view, std::reference_wrapper<SkeletonEventBase>>;
    using SkeletonFields = std::map<score::cpp::string_view, std::reference_wrapper<SkeletonFieldBase>>;

    /// \brief Creation of service skeleton with provided Skeleton binding
    ///
    /// \requirement SWS_CM_00130
    ///
    /// \param skeleton_binding The SkeletonBinding which is created using SkeletonBindingFactory.
    /// \param instance_id The instance identifier which uniquely identifies this Skeleton instance.
    /// \param mode As a default argument, this is the mode of the service implementation for processing service method
    /// invocations with kEvent as default value. See [SWS_CM_00301] for the type definition and [SWS_CM_00198] for more
    /// details on the behavior
    explicit SkeletonBase(std::unique_ptr<SkeletonBinding> skeleton_binding,
                          const InstanceIdentifier instance_id,
                          MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent);

    virtual ~SkeletonBase();

    /// \brief A Skeleton shall not be copyable
    /// \requirement SWS_CM_00134
    SkeletonBase(const SkeletonBase&) = delete;
    SkeletonBase& operator=(const SkeletonBase&) = delete;

    /// \brief Offer the respective service to other applications
    ///
    /// \return On failure, returns an error code according to the SW Component Requirements SCR-17434118 and
    /// SCR-566325.
    [[nodiscard]] ResultBlank OfferService() noexcept;

    /// \brief Stops offering the respective service to other applications
    ///
    /// \requirement SWS_CM_00111
    void StopOfferService() noexcept;

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
    InstanceIdentifier instance_id_;

    /// \brief Perform required clean up operations when a SkeletonBase object is destroyed or overwritten (by the move
    /// assignment operator)
    void Cleanup();

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

    void RegisterEvent(const score::cpp::string_view event_name, SkeletonEventBase& event)
    {
        const auto result = skeleton_base_.events_.emplace(event_name, event);
        const bool was_event_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_event_inserted, "Event cannot be registered as it already exists.");
    }

    void RegisterField(const score::cpp::string_view field_name, SkeletonFieldBase& field)
    {
        const auto result = skeleton_base_.fields_.emplace(field_name, field);
        const bool was_field_inserted = result.second;
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_field_inserted, "Field cannot be registered as it already exists.");
    }

    // Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
    // called implicitly". std::terminate() is implicitly called from std::map::at in case the container doesn't have
    // the mapped element. This function is called by the move constructor and as we register the event name in the
    // events_ container during SkeletonEvent construction so we are sure that the event_name already exists, so no way
    // for throwing std::out_of_range which leds to std::terminate().
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    void UpdateEvent(const score::cpp::string_view event_name, SkeletonEventBase& event) noexcept
    {
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "If a function is declared to be
        // noexcept, noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception"
        // This function is indirectly called by the move constructor and as we register the event name in the
        // events_ container during SkeletonEvent construction so we are sure the event name already exists, so no way
        // for throwing std::out_of_range which leds to std::terminate().
        // coverity[autosar_cpp14_a15_4_2_violation] : FALSE]
        skeleton_base_.events_.at(event_name) = event;
    }

    void UpdateField(const score::cpp::string_view field_name, SkeletonFieldBase& field) noexcept
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

    const SkeletonBase::SkeletonEvents& GetEvents() const noexcept
    {
        return skeleton_base_.events_;
    }

    const SkeletonBase::SkeletonFields& GetFields() const noexcept
    {
        return skeleton_base_.fields_;
    }

  private:
    SkeletonBase& skeleton_base_;
};

score::cpp::optional<InstanceIdentifier> GetInstanceIdentifier(const InstanceSpecifier& specifier);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_BASE_H
