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

#ifndef SCORE_MW_SERVICE_PROXY_BUILDER_BASE_H
#define SCORE_MW_SERVICE_PROXY_BUILDER_BASE_H

#include "score/mw/service/details/proxy_spec_traits.h"

#include "score/mw/service/proxy_future.h"

#include <score/callback.hpp>
#include <score/stop_token.hpp>

#include <mutex>
#include <optional>
#include <utility>
#include <variant>

namespace score::mw::service
{

/// @brief ProxyBuilderBase describes an interface that can be used to abstract the way how proxies are built.
///
/// @note A user should not directly use ProxyBuilderBase, he should rather use `ProxyNeeds` including
/// `ProxyNeedsFactory`. This interface is nevertheless public because a user needs to reference it when implementing
/// custom strategies.
///
/// @tparam ProxySpec describes the list of proxy interfaces (abstract) that shall be built by this builder.
template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class ProxyBuilderBase
{
  public:
    /// @brief The BuilderCallback invokes the construction of the concrete type (derivation of ProxyBase) only once in
    /// a thread-safe manner. This means that it is ok to invoke this from different threads in parallel with the class
    /// being constructed only once (the first one reaching it).
    using BuilderCallback = typename details::ProxySpecTraits<ProxySpec>::BuilderCallback;

    /// @brief A callback to be provided by the user. It will get invoked once a proxy's corresp. service got found.
    using UserCallback = typename details::ProxySpecTraits<ProxySpec>::UserCallback;

    /// @brief An initialization callback to be provided by the user to initialize a Proxy. It will get invoked once a
    /// proxy's corresp. service got found.
    ///
    /// This is an alternative to UserCallback which allows returning an error from the callback which can be used to
    /// inhibit the creation of a proxy instance. If an error is returned, the ResolvedType (determined based on the
    /// ProxyNeeds) stored in ProxyContainer will contain:
    ///   * Mandatory proxy (ResolvedType == SingleInstanceHolder: The SingleInstanceHolder will contain a nullptr.
    ///   * Optional proxy (ResolvedType == OptionalProxyData<ProxyType>: The future (returned over
    ///     GetProxyFuture() interface) will contain an error.
    ///   * Multiple proxy (ResolvedType == MultipleProxyData<ProxyType>: The returned MultiInstanceHolder
    ///     (using GetProxyInstances() API) could be empty and might not contain any created proxy at this point.
    ///   * Variant proxy (ResolvedType == std::variant: Currently, a default-constructed variant of the user will be
    ///     returned which will fail to build since it's not default constructible. This will need to be revised in the
    ///     future.
    using InitCallback = typename details::ProxySpecTraits<ProxySpec>::InitCallback;

    ProxyBuilderBase() noexcept = default;

    virtual ~ProxyBuilderBase() = default;

    /// @brief Starts service discovery and builds a proxy once found (in an asynchronous matter)
    ///
    /// @note How a proxy is found, and how the concrete (derived) instance of ProxyBase is constructed, is determined
    /// by the Strategies. Any provided user-callback will not be available on the next `Build()` invocation.
    ///
    /// @return mw::service::ProxyFuture<SingleInstanceHolder<ProxyBase>> if `ProxySpec` is a single element,
    ///         mw::service::ProxyFuture<std::variant<SingleInstanceHolder<ProxySpec>...>> otherwise.
    [[nodiscard]] virtual auto Build(std::optional<score::cpp::stop_token>) ->
        typename details::ProxySpecTraits<ProxySpec>::BuilderReturn = 0;

    /// @brief Option for users to provide a callback. It will get invoked once a proxy's corresp. service got found.
    ///
    /// If WithOnServiceFound is called a second time with a UserCallback or InitCallback, the callback will not be
    /// registered.
    ///
    /// @param user_callback The callback that shall be invoked.
    void WithOnServiceFound(UserCallback user_callback) noexcept
    {
        using NoUserCallback = typename details::ProxySpecTraits<ProxySpec>::NoUserCallback;
        if (std::holds_alternative<NoUserCallback>(callback_))
        {
            callback_ = std::move(user_callback);
        }
    }

    /// @brief Option for users to provide an initialization callback. It will get invoked once a proxy's corresp.
    /// service got found.
    ///
    /// If WithOnServiceFound is called a second time with a UserCallback or InitCallback, the callback will not be
    /// registered.
    ///
    /// @param user_callback The callback that shall be invoked.
    void WithOnServiceFound(InitCallback init_callback) noexcept
    {
        using NoUserCallback = typename details::ProxySpecTraits<ProxySpec>::NoUserCallback;
        if (std::holds_alternative<NoUserCallback>(callback_))
        {
            callback_ = std::move(init_callback);
        }
    }

  protected:
    ProxyBuilderBase(const ProxyBuilderBase&) noexcept = default;
    ProxyBuilderBase(ProxyBuilderBase&&) noexcept = default;
    ProxyBuilderBase& operator=(const ProxyBuilderBase& other) noexcept = default;
    ProxyBuilderBase& operator=(ProxyBuilderBase&& other) noexcept = default;
    // coverity[autosar_cpp14_m11_0_1_violation] False positive, member is used in derived class
    typename details::ProxySpecTraits<ProxySpec>::UserCallbackVariant callback_{};
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_PROXY_BUILDER_BASE_H
