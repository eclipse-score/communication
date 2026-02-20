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
#include "score/mw/com/impl/service_discovery.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/i_binding_runtime.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{

ServiceDiscovery::ServiceDiscovery(score::mw::com::impl::IRuntime& runtime)
    : IServiceDiscovery{},
      runtime_{runtime},
      next_free_uid_{0U},
      container_mutex_{},
      user_callbacks_{},
      handle_to_instances_{}
{
}

ServiceDiscovery::~ServiceDiscovery()
{
    const auto copy_of_handles = handle_to_instances_;
    for (const auto& handle : copy_of_handles)
    {
        const auto result = ServiceDiscovery::StopFindService(handle.first);
        if (!result.has_value())
        {
            mw::log::LogError("lola") << "FindService for (" << FindServiceHandleView{handle.first}.getUid() << ","
                                      << handle.second.GetInstanceIdentifier().ToString() << ") could not be stopped"
                                      << result.error();
        }
    }
}

auto ServiceDiscovery::OfferService(score::mw::com::impl::InstanceIdentifier instance_identifier) noexcept -> ResultBlank
{
    auto& service_discovery_client = GetServiceDiscoveryClient(instance_identifier);

    return service_discovery_client.OfferService(std::move(instance_identifier));
}

auto ServiceDiscovery::StopOfferService(score::mw::com::impl::InstanceIdentifier instance_identifier) noexcept
    -> ResultBlank
{
    return StopOfferService(instance_identifier, QualityTypeSelector::kBoth);
}

auto ServiceDiscovery::StopOfferService(score::mw::com::impl::InstanceIdentifier instance_identifier,
                                        QualityTypeSelector quality_type) noexcept -> ResultBlank
{
    auto& service_discovery_client = GetServiceDiscoveryClient(instance_identifier);

    return service_discovery_client.StopOfferService(std::move(instance_identifier), quality_type);
}

auto ServiceDiscovery::StartFindService(FindServiceHandler<HandleType> handler,
                                        const InstanceSpecifier instance_specifier) noexcept
    -> Result<FindServiceHandle>
{
    const auto instance_identifiers = runtime_.resolve(instance_specifier);
    const std::vector<EnrichedInstanceIdentifier> enriched_instance_identifiers(instance_identifiers.begin(),
                                                                                instance_identifiers.end());
    const auto find_service_handle = GetNextFreeFindServiceHandle();

    // Get the user callback and store the instance identifiers under lock to ensure that the
    // underlying data structures are not modified while we're accessing them. However, StartFindService is called on
    // the binding without locking container_mutex_ to prevent deadlocks between different calls to StartFindService /
    // StopFindService (see Ticket-169333). ServiceDiscoveryClient synchronises these calls itself.
    std::unique_lock lock{container_mutex_};
    auto handler_weak_ptr = StoreUserCallback(find_service_handle, std::move(handler));

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(handle_to_instances_.count(find_service_handle) == 0U, "FindServiceHandle is not unique!");
    for (const auto& enriched_instance_identifier : enriched_instance_identifiers)
    {
        StoreInstanceIdentifier(find_service_handle, enriched_instance_identifier);
    }
    lock.unlock();

    auto current_identifier_it = enriched_instance_identifiers.cbegin();
    for (; current_identifier_it != enriched_instance_identifiers.cend(); ++current_identifier_it)
    {
        auto result = StartFindServiceImpl(find_service_handle, handler_weak_ptr, *current_identifier_it);
        // If the binding StartFindService fails, then we don't continue calling StartFindService on any other
        // bindings
        if (!(result.has_value()))
        {
            // Remove the instance identifiers from handle_to_instances_ for all bindings on which we never called
            // StartFindService (since we're exiting early here)
            RemoveUnusedInstanceIdentifiers(
                find_service_handle, current_identifier_it, enriched_instance_identifiers.cend());
            const auto stop_find_service_result = StopFindService(find_service_handle);
            if (!(stop_find_service_result.has_value()))
            {
                mw::log::LogError("lola")
                    << "StopFindService after StartFindService with InstanceSpecifier failed on binding failed for ("
                    << FindServiceHandleView{find_service_handle}.getUid() << ","
                    << current_identifier_it->GetInstanceIdentifier().ToString() << ") could not be stopped."
                    << result.error();
            }

            return result;
        }
    }

    return find_service_handle;
}

auto ServiceDiscovery::StartFindService(FindServiceHandler<HandleType> handler,
                                        InstanceIdentifier instance_identifier) noexcept -> Result<FindServiceHandle>
{
    EnrichedInstanceIdentifier enriched_instance_identifier{std::move(instance_identifier)};
    return StartFindService(std::move(handler), std::move(enriched_instance_identifier));
}

auto ServiceDiscovery::StartFindService(FindServiceHandler<HandleType> handler,
                                        const EnrichedInstanceIdentifier enriched_instance_identifier) noexcept
    -> Result<FindServiceHandle>
{
    auto find_service_handle = GetNextFreeFindServiceHandle();

    // Get the user callback and store the instance identifier under lock to ensure that the
    // underlying data structures are not modified while we're accessing them. However, StartFindService is called on
    // the binding without locking container_mutex_ to prevent deadlocks between different calls to StartFindService /
    // StopFindService (see Ticket-169333). ServiceDiscoveryClient synchronises these calls itself.
    std::unique_lock lock{container_mutex_};
    auto handler_weak_ptr = StoreUserCallback(find_service_handle, std::move(handler));
    StoreInstanceIdentifier(find_service_handle, enriched_instance_identifier);
    lock.unlock();

    const auto result = StartFindServiceImpl(find_service_handle, handler_weak_ptr, enriched_instance_identifier);
    if (!(result.has_value()))
    {
        const auto stop_find_service_result = StopFindService(find_service_handle);
        if (!(stop_find_service_result.has_value()))
        {
            mw::log::LogError("lola")
                << "StopFindService after StartFindService with InstanceIdentifier failed on binding failed for ("
                << FindServiceHandleView{find_service_handle}.getUid() << ","
                << enriched_instance_identifier.GetInstanceIdentifier().ToString() << ") could not be stopped."
                << result.error();
        }
    }
    return result;
}

[[nodiscard]] auto ServiceDiscovery::StopFindService(const FindServiceHandle find_service_handle) noexcept
    -> ResultBlank
{
    // Make a copy of the EnrichedInstanceIdentifiers for which we need to call StopFindService. We make a copy under
    // lock to ensure that the map isn't modified while we're accessing it. However, StopFindService is called on the
    // binding without locking container_mutex_ to prevent deadlocks between different calls to StartFindService /
    // StopFindService (see Ticket-169333). ServiceDiscoveryClient synchronises these calls itself.
    std::unique_lock lock{container_mutex_};
    auto iterators = handle_to_instances_.equal_range(find_service_handle);
    std::vector<EnrichedInstanceIdentifier> enriched_instance_identifiers{};
    for (auto it = iterators.first; it != iterators.second; ++it)
    {
        enriched_instance_identifiers.push_back(it->second);
    }

    score::cpp::ignore = user_callbacks_.erase(find_service_handle);
    score::cpp::ignore = handle_to_instances_.erase(find_service_handle);
    lock.unlock();

    ResultBlank result{};
    for (const auto& enriched_instance_identifier : enriched_instance_identifiers)
    {
        auto& service_discovery_client =
            GetServiceDiscoveryClient(enriched_instance_identifier.GetInstanceIdentifier());
        auto specific_result = service_discovery_client.StopFindService(find_service_handle);
        if (!specific_result.has_value())
        {
            result = specific_result;
        }
    }

    return result;
}

auto ServiceDiscovery::StartFindServiceImpl(FindServiceHandle find_service_handle,
                                            std::weak_ptr<FindServiceHandler<HandleType>> handler_weak_ptr,
                                            const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> Result<FindServiceHandle>
{
    auto result = BindingSpecificStartFindService(find_service_handle, handler_weak_ptr, enriched_instance_identifier);
    if (!result.has_value())
    {
        return Unexpected{result.error()};
    }

    return find_service_handle;
}

void ServiceDiscovery::RemoveUnusedInstanceIdentifiers(
    FindServiceHandle find_service_handle,
    std::vector<EnrichedInstanceIdentifier>::const_iterator last_used_iterator,
    std::vector<EnrichedInstanceIdentifier>::const_iterator container_end) noexcept
{
    const bool all_identifiers_processed{last_used_iterator == container_end};
    // GCOV_EXCL_START : This is a never true. Defensive programming: This function is only called by StartFindService.
    // Within StartFindService, we check that last_used_iterator is not equal to container_end and only call this
    // function if that's not the case.
    if (all_identifiers_processed)
    {
        return;
    }
    // GCOV_EXCL_STOP
    last_used_iterator++;
    const auto unused_identifiers = std::make_pair(last_used_iterator, container_end);

    // Suppress Autosar C++14 A8-5-3 states that "auto variables shall not be initialized using braced initialization".
    // This is a false positive, we don't use auto here.
    // coverity[autosar_cpp14_a8_5_3_violation : FALSE]
    const std::lock_guard lock_guard{container_mutex_};
    for (auto unused_identifier_it = unused_identifiers.first; unused_identifier_it != unused_identifiers.second;
         ++unused_identifier_it)
    {
        const auto handle_to_instances_iterators = handle_to_instances_.equal_range(find_service_handle);
        auto container_it = handle_to_instances_iterators.first;
        while (container_it != handle_to_instances_iterators.second)
        {
            if (*unused_identifier_it == container_it->second)
            {
                container_it = handle_to_instances_.erase(container_it);
            }
            else
            {
                container_it++;
            }
        }
    }
}

FindServiceHandle ServiceDiscovery::GetNextFreeFindServiceHandle() noexcept
{
    // This is an atomic value. Incrementing and assignement has to take place on the same line.
    // Since there are no other operations besides increment ans assignement, it is quite clear what is happening.
    // coverity[autosar_cpp14_m5_2_10_violation]
    const auto free_uid = next_free_uid_++;
    return make_FindServiceHandle(free_uid);
}

auto ServiceDiscovery::StoreUserCallback(const FindServiceHandle& find_service_handle,
                                         FindServiceHandler<HandleType> handler) noexcept
    -> std::weak_ptr<FindServiceHandler<HandleType>>
{
    auto shared_pointer_handler_wrapper = std::make_shared<FindServiceHandler<HandleType>>(std::move(handler));
    auto entry = user_callbacks_.emplace(std::piecewise_construct,
                                         std::forward_as_tuple(find_service_handle),
                                         std::forward_as_tuple(std::move(shared_pointer_handler_wrapper)));
    return entry.first->second;
}

auto ServiceDiscovery::StoreInstanceIdentifier(const FindServiceHandle& find_service_handle,
                                               const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> void
{
    score::cpp::ignore = handle_to_instances_.emplace(std::piecewise_construct,
                                               std::forward_as_tuple(find_service_handle),
                                               std::forward_as_tuple(enriched_instance_identifier));
}

auto ServiceDiscovery::GetServiceDiscoveryClient(const InstanceIdentifier& instance_identifier) noexcept
    -> IServiceDiscoveryClient&
{
    InstanceIdentifierView instance_identifier_view{instance_identifier};
    auto binding_type = instance_identifier_view.GetServiceInstanceDeployment().GetBindingType();
    auto binding_runtime = runtime_.GetBindingRuntime(binding_type);

    if (binding_runtime == nullptr)
    {
        mw::log::LogFatal("lola") << "Service discovery failed to find fitting binding for"
                                  << instance_identifier.ToString();
    }
    // TM: This assertion is redundant (Line 298)
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(binding_runtime != nullptr, "Unsupported binding"); 

    return binding_runtime->GetServiceDiscoveryClient();
}

auto ServiceDiscovery::BindingSpecificStartFindService(
    FindServiceHandle search_handle,
    std::weak_ptr<FindServiceHandler<HandleType>> handler_weak_ptr,
    const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept -> ResultBlank
{
    auto& service_discovery_client = GetServiceDiscoveryClient(enriched_instance_identifier.GetInstanceIdentifier());

    return service_discovery_client.StartFindService(
        search_handle,
        [handler_weak_ptr](auto container, auto handle) noexcept {
            if (auto handler_shared_ptr = handler_weak_ptr.lock())
            {
                (*handler_shared_ptr)(container, handle);
            }
        },
        enriched_instance_identifier);
}

Result<ServiceHandleContainer<HandleType>> ServiceDiscovery::FindService(
    InstanceIdentifier instance_identifier) noexcept
{
    EnrichedInstanceIdentifier enriched_instance_identifier{std::move(instance_identifier)};
    auto& service_discovery_client = GetServiceDiscoveryClient(enriched_instance_identifier.GetInstanceIdentifier());
    const auto find_service_result = service_discovery_client.FindService(std::move(enriched_instance_identifier));
    if (!(find_service_result.has_value()))
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return find_service_result;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'result.has_value()' in case the
// result doesn't have a value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access
// which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Result<ServiceHandleContainer<HandleType>> ServiceDiscovery::FindService(InstanceSpecifier instance_specifier) noexcept
{
    ServiceHandleContainer<HandleType> handles;
    const auto instance_identifiers = runtime_.resolve(instance_specifier);
    if (instance_identifiers.size() == static_cast<std::size_t>(0U))
    {
        score::mw::log::LogError("lola") << "Failed to resolve instance identifier from instance specifier";
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }

    bool are_only_errors_received{true};
    for (auto instance_identifier : instance_identifiers)
    {
        auto result = FindService(std::move(instance_identifier));
        if (result.has_value())
        {
            are_only_errors_received = false;
            for (const auto& handle : result.value())
            {
                handles.push_back(handle);
            }
        }
    }

    if (are_only_errors_received)
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return handles;
}

}  // namespace score::mw::com::impl
