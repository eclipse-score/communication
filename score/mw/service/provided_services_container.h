/*********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_PROVIDED_SERVICES_CONTAINER_H
#define SCORE_MW_SERVICE_PROVIDED_SERVICES_CONTAINER_H

#include "score/mw/service/managed_service.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::service
{

/// @brief Container that owns a set of `ManagedService` instances and manages their lifecycle.
///
/// Each service is started as soon as it gets added via `Emplace()`. All services which are still
/// held by the container get either stopped explicitly via `StopAll()` or implicitly when the
/// container is destroyed (i.e. goes out of scope).
///
/// Pointers returned by `Get()`/`Get<>()` refer to container-owned storage and remain
/// valid only until the particular service instance is still held by the container, i.e. it did
/// not get extracted and this container did not get destroyed.
///
/// A pointer becomes invalid once the specific service it refers to is destroyed. This happens on
/// `StopAll()` and on destruction of this container, both of which call `Stop()` on and destroy every
/// remaining service. It also happens once the caller destroys a service previously obtained via
/// `Extract()`/`Extract()` for that same service: `Extract()` transfers ownership to the
/// caller without destroying the object, so the container can no longer vouch for its lifetime
/// afterwards, and the pointer must be treated as invalidated from that call onward.
///
/// @attention This class is not thread-safe. It does not provide any internal synchronization. In
/// particular, none of its member functions may be called concurrently with each other from different
/// threads without external synchronization. This includes `Get()`/`Get()` running
/// concurrently with `Extract()`/`Extract()`/`StopAll()`: even if the
/// container's internal storage were protected, a pointer returned by `Get()` can be invalidated at any
/// time by a concurrent `Extract()` of that same service, or by a concurrent `StopAll()`, on another
/// thread, since ownership of (or, for `StopAll()`, the lifetime of) the pointed-to object may be
/// transferred out (and subsequently destroyed) or destroyed outright without the holder of the pointer
/// being notified. Callers are responsible for ensuring that a pointer obtained from `Get()`/`Get()`
/// is not used after any such invalidating call has been made, and must not treat a previously
/// validated (non-null) pointer as still valid without re-validating it (e.g. by calling
/// `Get()`/`Get()` again) after any invalidating call could have occurred.
class ProvidedServicesContainer
{
    using ServiceIdentifierType = std::string;
    using ServiceIdentifierView = std::string_view;
    using ManagedServiceHolder = std::unique_ptr<ManagedService>;

  public:
    ProvidedServicesContainer() noexcept = default;
    ~ProvidedServicesContainer() noexcept;

    ProvidedServicesContainer(const ProvidedServicesContainer&) = delete;
    ProvidedServicesContainer(ProvidedServicesContainer&&) noexcept = default;
    ProvidedServicesContainer& operator=(const ProvidedServicesContainer&) = delete;

    /// @brief Move-assign from another container.
    /// @details `*this` must not already hold any services, since a defaulted move-assignment would otherwise
    ///          silently destroy them without calling `Stop()` on them first. Callers who need to replace a
    ///          container that still holds services must call `StopAll()` (or extract every service) before
    ///          move-assigning into it.
    ProvidedServicesContainer& operator=(ProvidedServicesContainer&& other) &
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            services_.empty(),
            "Cannot move-assign into a ProvidedServicesContainer that still holds services, "
            "call StopAll() (or extract every service) first!");
        services_ = std::move(other.services_);
        return *this;
    }

    /// @brief Construct a new `ServiceType` instance in-place and start it immediately.
    /// @details If starting the newly constructed service fails, it is discarded (not added to the
    ///          container) and the failure is propagated to the caller.
    /// @tparam ServiceType the (implementation) type of the service to be added, must inherit from `ManagedService`
    /// @tparam ...Args types of the provided arguments forwarded to the `ServiceType` constructor
    /// @param service_identifier unique identifier for the service instance
    /// @param ...args values of the provided arguments forwarded to the `ServiceType` constructor
    /// @return an empty `Result<void>` on success, or the error returned by `ServiceType::Start()` on failure
    template <typename ServiceType, typename... Args>
    Result<void> Emplace(ServiceIdentifierView service_identifier, Args&&... args)
    {
        static_assert(std::is_base_of_v<ManagedService, ServiceType>,
                      "Specified ServiceType must inherit from 'mw::service::ManagedService'");

        // in order to give the strong exception guarantee,
        // increase the capacity (which might throw) before we attempt to start the service
        services_.reserve(services_.size() + 1U);

        auto service = std::make_unique<ServiceType>(std::forward<Args>(args)...);
        if (auto start_result = service->Start(); !start_result.has_value())
        {
            return start_result;
        }

        services_.emplace_back(ServiceIdentifierType{service_identifier}, std::move(service));

        return {};
    }

    /// @brief Construct a new `ServiceType` instance in-place and start it immediately (w/o any service identifier).
    /// @details If starting the newly constructed service fails, it is discarded (not added to the container) and
    ///          the failure is propagated to the caller.
    /// @tparam ServiceType the (implementation) type of the service to be added, must inherit from `ManagedService`
    /// @tparam Arg type of the first argument forwarded to the `ServiceType` constructor
    /// @tparam ...Args types of the remaining arguments forwarded to the `ServiceType` constructor
    /// @tparam  enable_if condition to ensure the first argument is not convertible to `ServiceIdentifierView`
    /// @param arg value of the first argument forwarded to the `ServiceType` constructor
    /// @param ...args values of the remaining arguments forwarded to the `ServiceType` constructor
    /// @return an empty `Result<void>` on success, or the error returned by `ServiceType::Start()` on failure
    template <
        typename ServiceType,
        typename Arg,
        typename... Args,
        std::enable_if_t<std::negation_v<std::is_convertible<std::decay_t<Arg>, ServiceIdentifierView>>, bool> = true>
    Result<void> Emplace(Arg&& arg, Args&&... args)
    {
        static_assert(std::is_base_of_v<ManagedService, ServiceType>,
                      "Specified ServiceType must inherit from 'mw::service::ManagedService'");

        return Emplace<ServiceType>(ServiceIdentifierView{}, std::forward<Arg>(arg), std::forward<Args>(args)...);
    }

    /// @brief Construct a new `ServiceType` instance in-place and start it immediately (w/o any service identifier).
    /// @details If starting the newly constructed service fails, it is discarded (not added to the container) and
    ///          the failure is propagated to the caller.
    /// @tparam ServiceType the (implementation) type of the service to be added, must inherit from `ManagedService`
    /// @return an empty `Result<void>` on success, or the error returned by `ServiceType::Start()` on failure
    template <typename ServiceType>
    Result<void> Emplace()
    {
        static_assert(std::is_base_of_v<ManagedService, ServiceType>,
                      "Specified ServiceType must inherit from 'mw::service::ManagedService'");

        return Emplace<ServiceType>(ServiceIdentifierView{});
    }

    /// @brief Extract the first contained service instance matching a particular (implementation) type
    /// @details The extracted service is removed from the container and ownership is transferred to the caller
    ///          without calling `Stop()` on it. It is the caller's responsibility to call `Stop()` on the
    ///          extracted service once it should no longer be offered.
    /// @tparam ServiceType the expected (implementation) type of the service instance to be extracted
    /// @return a valid `unique_ptr<ServiceType>` in case a stored service can be dynamic_cast to `ServiceType`,
    /// nullptr otherwise
    template <typename ServiceType>
    std::unique_ptr<ServiceType> Extract() noexcept
    {
        const auto it = FindService<ServiceType>(ServiceIdentifierView{});
        if (it == services_.end())
        {
            return nullptr;
        }
        std::unique_ptr<ServiceType> extracted_service{
            static_cast<ServiceType*>(std::get<ManagedServiceHolder>(*it).release())};
        services_.erase(it);
        return extracted_service;
    }

    /// @brief Extract the first contained service instance matching a particular service identifier and
    ///        (implementation) type
    /// @details The extracted service is removed from the container and ownership is transferred to the caller
    ///          without calling `Stop()` on it. It is the caller's responsibility to call `Stop()` on the
    ///          extracted service once it should no longer be offered.
    /// @tparam ServiceType the expected (implementation) type of the service instance to be extracted
    /// @param service_identifier identifier for the service instance, must not be empty (use `Extract()` for
    /// type-based lookup instead)
    /// @return a valid `unique_ptr<ServiceType>` in case a stored service matching `service_identifier` can be
    /// dynamic_cast to `ServiceType`, nullptr otherwise
    template <typename ServiceType>
    std::unique_ptr<ServiceType> Extract(ServiceIdentifierView service_identifier)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            !service_identifier.empty(), "service_identifier must not be empty, use Extract() for type-based lookup");
        const auto it = FindService<ServiceType>(service_identifier);
        if (it == services_.end())
        {
            return nullptr;
        }
        std::unique_ptr<ServiceType> extracted_service{
            static_cast<ServiceType*>(std::get<ManagedServiceHolder>(*it).release())};
        services_.erase(it);
        return extracted_service;
    }

    /// @brief Get the first contained service instance matching a particular (implementation) type
    /// @details Not thread-safe: see the class-level note. The returned pointer stays valid across
    ///          `Emplace()`, moving this container, and `Extract()`ing a different service, but becomes
    ///          invalid once this particular service is stopped and destroyed via `StopAll()`, container
    ///          destruction, or disposal of the object after extracting it via `Extract()`/`Extract()`.
    ///          Callers must re-validate (call `Get()` again) the pointer after any such invalidating call.
    /// @tparam ServiceType the expected (implementation) type of the service instance to be found
    /// @return a pointer to the service in case a stored service can be dynamic_cast to `ServiceType`, nullptr
    /// otherwise
    template <typename ServiceType>
    ServiceType* Get() noexcept
    {
        const auto it = FindService<ServiceType>(ServiceIdentifierView{});
        return it != services_.end() ? static_cast<ServiceType*>(std::get<ManagedServiceHolder>(*it).get()) : nullptr;
    }
    template <typename ServiceType>
    const ServiceType* Get() const noexcept
    {
        const auto it = FindService<ServiceType>(ServiceIdentifierView{});
        return it != services_.end() ? static_cast<const ServiceType*>(std::get<ManagedServiceHolder>(*it).get())
                                     : nullptr;
    }

    /// @brief Get the first contained service instance matching a particular service identifier and
    ///        (implementation) type
    /// @details Not thread-safe: see the class-level note. The returned pointer stays valid across
    ///          `Emplace()`, moving this container, and `Extract()`ing a different service, but becomes
    ///          invalid once this particular service is stopped and destroyed via `StopAll()`, container
    ///          destruction, or disposal of the object after extracting it via `Extract()`/`Extract()`.
    ///          Callers must re-validate (call `Get()` again) the pointer after any such invalidating call.
    /// @tparam ServiceType the expected (implementation) type of the service instance to be found
    /// @param service_identifier identifier for the service instance, must not be empty (use `Get()` for
    /// type-based lookup instead)
    /// @return a pointer to the service in case a stored service matching `service_identifier` can be
    /// dynamic_cast to `ServiceType`, nullptr otherwise
    template <typename ServiceType>
    ServiceType* Get(ServiceIdentifierView service_identifier)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            !service_identifier.empty(), "service_identifier must not be empty, use Get() for type-based lookup");
        const auto it = FindService<ServiceType>(service_identifier);
        return it != services_.end() ? static_cast<ServiceType*>(std::get<ManagedServiceHolder>(*it).get()) : nullptr;
    }

    // const overload: returns `const ServiceType*` so that a `const ProvidedServicesContainer` cannot be used
    // to obtain a mutable pointer to a contained service.
    template <typename ServiceType>
    const ServiceType* Get(ServiceIdentifierView service_identifier) const
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            !service_identifier.empty(), "service_identifier must not be empty, use Get() for type-based lookup");
        const auto it = FindService<ServiceType>(service_identifier);
        return it != services_.end() ? static_cast<const ServiceType*>(std::get<ManagedServiceHolder>(*it).get())
                                     : nullptr;
    }

    /// @brief Check whether a service instance with a particular (implementation) type exists
    /// @tparam ServiceType the expected (implementation) type of the service instance whose existence to check
    /// @return bool indicating whether a service instance with the given type exists
    template <typename ServiceType>
    bool Has() const noexcept
    {
        return FindService<ServiceType>(ServiceIdentifierView{}) != services_.end();
    }

    /// @brief Check whether a service instance with a particular service identifier and (implementation)
    ///        type exists
    /// @tparam ServiceType the expected (implementation) type of the service instance whose existence to check
    /// @param service_identifier identifier for the service instance, must not be empty (use `Has()` for
    /// type-based lookup instead)
    /// @return bool indicating whether a service instance matching `service_identifier` and the given type exists
    template <typename ServiceType>
    bool Has(ServiceIdentifierView service_identifier) const
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            !service_identifier.empty(), "service_identifier must not be empty, use Has() for type-based lookup");
        return FindService<ServiceType>(service_identifier) != services_.end();
    }

    /// @brief Get the total number of services contained in this container
    inline std::size_t Size() const noexcept
    {
        return services_.size();
    }

    /// @brief Reserve storage for a known number of service instances up front, avoiding repeated
    ///        reallocations when many services are added via `Emplace()`.
    /// @param size the number of service instances to reserve storage for
    void Reserve(std::size_t size);

    /// @brief Stop all contained service instances and remove them from this container
    void StopAll() noexcept;

  private:
    /// @brief Find the first entry matching a particular service identifier and (implementation) type.
    /// @tparam ServiceType the expected (implementation) type of the service instance to be found
    /// @param service_identifier identifier for the service instance, an empty identifier matches any service
    /// @return an iterator to the first matching entry, or `services_.end()` if none matches
    /// @note An empty `service_identifier` is only used for type-based lookup performed by `Get()`/`Has()`.
    /// Public API validation ensures that `Get()`/`Has()`, which perform
    /// identifier-based lookup, are never called with an empty identifier.
    template <typename ServiceType>
    auto FindService(ServiceIdentifierView service_identifier) noexcept
    {
        return std::find_if(services_.begin(), services_.end(), [service_identifier](const auto& entry) {
            return (service_identifier.empty() || std::get<ServiceIdentifierType>(entry) == service_identifier) &&
                   (dynamic_cast<ServiceType*>(std::get<ManagedServiceHolder>(entry).get()) != nullptr);
        });
    }

    template <typename ServiceType>
    auto FindService(ServiceIdentifierView service_identifier) const noexcept
    {
        return std::find_if(services_.begin(), services_.end(), [service_identifier](const auto& entry) {
            return (service_identifier.empty() || std::get<ServiceIdentifierType>(entry) == service_identifier) &&
                   (dynamic_cast<const ServiceType*>(std::get<ManagedServiceHolder>(entry).get()) != nullptr);
        });
    }

    std::vector<std::tuple<ServiceIdentifierType, ManagedServiceHolder>> services_;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROVIDED_SERVICES_CONTAINER_H
