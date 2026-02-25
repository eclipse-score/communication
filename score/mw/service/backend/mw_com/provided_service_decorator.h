// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_DECORATOR_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_DECORATOR_H

#include <memory>
#include <utility>

namespace score
{
namespace mw
{
namespace service
{
namespace backend
{
namespace mw_com
{

/// @brief Simplified stub decorator for provided services
/// @tparam ServiceType The service implementation type
template <typename ServiceType>
class ProvidedServiceDecorator
{
  public:
    using ServiceHolder = std::unique_ptr<ServiceType>;

    ProvidedServiceDecorator() = default;
    ~ProvidedServiceDecorator() = default;

    ProvidedServiceDecorator(const ProvidedServiceDecorator&) = delete;
    ProvidedServiceDecorator(ProvidedServiceDecorator&&) noexcept = default;
    ProvidedServiceDecorator& operator=(const ProvidedServiceDecorator&) = delete;
    ProvidedServiceDecorator& operator=(ProvidedServiceDecorator&&) noexcept = default;

    /// @brief Factory method to create a decorated service
    template <typename ServiceImplType = ServiceType, typename... Args>
    static ProvidedServiceDecorator Create(Args&&... args)
    {
        ProvidedServiceDecorator instance{};
        instance.service_ = std::make_unique<ServiceImplType>(std::forward<Args>(args)...);
        return instance;
    }

    /// @brief Get pointer to the service instance
    ServiceType* GetService() noexcept
    {
        return service_.get();
    }

    /// @brief Get const pointer to the service instance
    const ServiceType* GetService() const noexcept
    {
        return service_.get();
    }

    /// @brief Check if a specific service type exists (stub - always returns false)
    template <typename T>
    constexpr bool Has() const noexcept
    {
        return false;
    }

    /// @brief Get a pointer to the service as a specific type (stub - always returns nullptr)
    template <typename T>
    constexpr T* Get() noexcept
    {
        return nullptr;
    }

    /// @brief Get a const pointer to the service as a specific type (stub - always returns nullptr)
    template <typename T>
    constexpr const T* Get() const noexcept
    {
        return nullptr;
    }

  private:
    ServiceHolder service_;
};

}  // namespace mw_com
}  // namespace backend
}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_DECORATOR_H
