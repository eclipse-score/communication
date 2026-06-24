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
#include "score/mw/com/impl/skeleton_base.h"

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/reference_to_moveable.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_binding.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/mw/com/impl/tracing/skeleton_tracing.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <algorithm>
#include <exception>
#include <functional>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

namespace
{

void StopOfferServiceInServiceDiscovery(const InstanceIdentifier& instance_identifier) noexcept
{
    const auto result = Runtime::getInstance().GetServiceDiscovery().StopOfferService(instance_identifier);
    if (!result.has_value())
    {
        score::mw::log::LogError("lola")
            << "SkeletonBinding::OfferService failed: service discovery could not stop offer"
            << result.error().Message() << ": " << result.error().UserMessage();
    }
}

SkeletonBinding::SkeletonEventBindings GetSkeletonEventBindingsMap(const SkeletonBase::SkeletonEvents& events)
{
    SkeletonBinding::SkeletonEventBindings event_bindings{};
    for (const auto& event : events)
    {
        const std::string_view event_name = event.first;
        SkeletonEventBase& skeleton_event_base = event.second.get().Get();

        auto skeleton_event_base_view = SkeletonEventBaseView{skeleton_event_base};
        auto* event_binding = skeleton_event_base_view.GetBinding();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            event_binding != nullptr, "Skeleton should not have been created if event binding failed to create.");
        score::cpp::ignore = event_bindings.insert({event_name, *event_binding});
    }
    return event_bindings;
}

SkeletonBinding::SkeletonFieldBindings GetSkeletonFieldBindingsMap(const SkeletonBase::SkeletonFields& fields)
{
    SkeletonBinding::SkeletonFieldBindings field_bindings{};
    for (const auto& field : fields)
    {
        const std::string_view field_name = field.first;
        SkeletonFieldBase& skeleton_field_base = field.second.get().Get();

        auto skeleton_field_base_view = SkeletonFieldBaseView{skeleton_field_base};
        auto* event_binding = skeleton_field_base_view.GetEventBinding();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            event_binding != nullptr, "Skeleton should not have been created if event binding failed to create.");
        score::cpp::ignore = field_bindings.insert({field_name, *event_binding});
    }
    return field_bindings;
}

}  // namespace

SkeletonBase::SkeletonBase(std::unique_ptr<SkeletonBinding> skeleton_binding, InstanceIdentifier instance_id)
    : binding_{std::move(skeleton_binding)},
      events_{},
      fields_{},
      methods_{},
      instance_id_{std::move(instance_id)},
      skeleton_mock_{nullptr},
      service_offered_flag_{}
{
}

score::Result<void> SkeletonBase::OfferServiceEvents() const noexcept
{
    for (const auto& event : events_)
    {
        const auto event_name = event.first;
        auto& skeleton_event = event.second.get().Get();
        const auto offer_result = skeleton_event.PrepareOffer();
        if (!offer_result.has_value())
        {
            score::mw::log::LogError("lola")
                << "SkeletonBinding::OfferService failed for event" << event_name
                << ": Reason:" << offer_result.error().Message() << ": " << offer_result.error().UserMessage();
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    return {};
}

score::Result<void> SkeletonBase::OfferServiceFields() const noexcept
{
    for (const auto& field : fields_)
    {
        const auto field_name = field.first;
        auto& skeleton_field = field.second.get().Get();
        const auto offer_result = skeleton_field.PrepareOffer();
        if (!offer_result.has_value())
        {
            score::mw::log::LogError("lola")
                << "SkeletonBinding::OfferService failed for field" << field_name
                << ": Reason:" << offer_result.error().Message() << ": " << offer_result.error().UserMessage();
            if (offer_result.error() == ComErrc::kFieldValueIsNotValid)
            {
                return MakeUnexpected(ComErrc::kFieldValueIsNotValid);
            }
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    return {};
}

auto SkeletonBase::OfferService() noexcept -> Result<void>
{
    if (skeleton_mock_ != nullptr)
    {
        return skeleton_mock_->OfferService();
    }

    if (binding_ == nullptr)
    {
        score::mw::log::LogFatal("lola") << "Trying to call OfferService() on a skeleton WITHOUT a binding!";
        std::terminate();
    }

    auto event_bindings = GetSkeletonEventBindingsMap(events_);
    auto field_bindings = GetSkeletonFieldBindingsMap(fields_);

    auto register_shm_object_callback =
        tracing::CreateRegisterShmObjectCallback(instance_id_, events_, fields_, *binding_);

    if (!binding_->VerifyAllMethodsRegistered())
    {
        constexpr std::string_view msg =
            "Not all methods have been registered! Call Register(...) with an appropriate callback on each mehtod.";
        return MakeUnexpected(ComErrc::kBindingFailure, msg);
    }

    const auto result = binding_->PrepareOffer(event_bindings, field_bindings, std::move(register_shm_object_callback));
    if (!result.has_value())
    {
        score::mw::log::LogError("lola") << "SkeletonBinding::OfferService failed: " << result.error().Message() << ": "
                                         << result.error().UserMessage();
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    const auto event_verification_result = OfferServiceEvents();
    if (!event_verification_result.has_value())
    {
        return event_verification_result;
    }

    const auto fields_verification_result = OfferServiceFields();
    if (!fields_verification_result.has_value())
    {
        return fields_verification_result;
    }

    service_offered_flag_.Set();

    const auto service_discovery_offer_result = Runtime::getInstance().GetServiceDiscovery().OfferService(instance_id_);
    if (!service_discovery_offer_result.has_value())
    {
        score::mw::log::LogError("lola")
            << "SkeletonBinding::OfferService failed: service discovery could not start offer"
            << service_discovery_offer_result.error().Message() << ": "
            << service_discovery_offer_result.error().UserMessage();
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return {};
}

auto SkeletonBase::StopOfferService() noexcept -> void
{
    if (skeleton_mock_ != nullptr)
    {
        skeleton_mock_->StopOfferService();
    }

    if (binding_ != nullptr && service_offered_flag_.IsSet())
    {
        StopOfferServiceInServiceDiscovery(instance_id_);

        for (auto& event : events_)
        {
            event.second.get().Get().PrepareStopOffer();
        }
        for (auto& field : fields_)
        {
            field.second.get().Get().PrepareStopOffer();
        }

        auto tracing_handler = tracing::CreateUnregisterShmObjectCallback(instance_id_, events_, fields_, *binding_);
        binding_->PrepareStopOffer(std::move(tracing_handler));
        service_offered_flag_.Clear();
        mw::log::LogInfo("lola") << "Service was stop offered successfully";
    }
}

auto SkeletonBase::AreBindingsValid() const noexcept -> bool
{
    const bool is_skeleton_binding_valid{binding_ != nullptr};
    bool are_service_element_bindings_valid{true};

    score::cpp::ignore =
        std::for_each(events_.begin(), events_.end(), [&are_service_element_bindings_valid](auto& element) {
            if (SkeletonEventBaseView{element.second.get().Get()}.GetBinding() == nullptr)
            {
                are_service_element_bindings_valid = false;
            }
        });
    score::cpp::ignore =
        std::for_each(fields_.begin(), fields_.end(), [&are_service_element_bindings_valid](auto& element) {
            if (SkeletonFieldBaseView{element.second.get().Get()}.GetEventBinding() == nullptr)
            {
                are_service_element_bindings_valid = false;
            }
        });

    score::cpp::ignore =
        std::for_each(methods_.begin(), methods_.end(), [&are_service_element_bindings_valid](auto& element) {
            if (SkeletonMethodBaseView{element.second.get().Get()}.GetMethodBinding() == nullptr)
            {
                are_service_element_bindings_valid = false;
            }
        });

    return is_skeleton_binding_valid && are_service_element_bindings_valid;
}

SkeletonBaseView::SkeletonBaseView(SkeletonBase& skeleton_base) : skeleton_base_{skeleton_base} {}

InstanceIdentifier SkeletonBaseView::GetAssociatedInstanceIdentifier() const
{
    return skeleton_base_.instance_id_;
}

SkeletonBinding* SkeletonBaseView::GetBinding() const
{
    return skeleton_base_.binding_.get();
}

void SkeletonBaseView::RegisterEvent(const std::string_view event_name,
                                     ReferenceToMoveable<SkeletonEventBase>::Reference& event)
{
    const auto result = skeleton_base_.events_.emplace(event_name, std::ref(event));
    const bool was_event_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_event_inserted, "Event cannot be registered as it already exists.");
}

void SkeletonBaseView::RegisterField(const std::string_view field_name,
                                     ReferenceToMoveable<SkeletonFieldBase>::Reference& field)
{
    const auto result = skeleton_base_.fields_.emplace(field_name, std::ref(field));
    const bool was_field_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_field_inserted, "Field cannot be registered as it already exists.");
}

void SkeletonBaseView::RegisterMethod(const std::string_view method_name,
                                      ReferenceToMoveable<SkeletonMethodBase>::Reference& method)
{
    const auto result = skeleton_base_.methods_.emplace(method_name, std::ref(method));
    const bool was_method_inserted = result.second;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(was_method_inserted, "Method cannot be registered as it already exists.");
}

const SkeletonBase::SkeletonEvents& SkeletonBaseView::GetEvents() const& noexcept
{
    return skeleton_base_.events_;
}

const SkeletonBase::SkeletonFields& SkeletonBaseView::GetFields() const& noexcept
{
    return skeleton_base_.fields_;
}

const SkeletonBase::SkeletonMethods& SkeletonBaseView::GetMethods() const& noexcept
{
    return skeleton_base_.methods_;
}

bool SkeletonBaseView::AreBindingsValid() const
{
    return skeleton_base_.AreBindingsValid();
}

score::cpp::optional<InstanceIdentifier> GetInstanceIdentifier(const InstanceSpecifier& specifier)
{
    const auto instance_identifiers = Runtime::getInstance().resolve(specifier);
    if (instance_identifiers.size() != static_cast<std::size_t>(1U))
    {
        return {};
    }
    auto instance_identifier = instance_identifiers.front();
    return instance_identifier;
}

}  // namespace score::mw::com::impl
