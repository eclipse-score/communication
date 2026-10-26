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

#ifndef SCORE_MW_SERVICE_FIND_SERVICE_STRATEGY_H
#define SCORE_MW_SERVICE_FIND_SERVICE_STRATEGY_H

#include "score/mw/service/proxy_builder_base.h"

#include <memory>

namespace score::mw::service
{

/// @brief Interface of a strategy which defines how a service is discovered and how the corresponding proxy instance(s)
///        get created. It is used by `ProxyBuilder` which drives the strategy on behalf of `ProxyNeeds`.
///
/// A strategy decouples *how* a service is found (middleware specific service discovery, immediate instantiation of
/// mocks for testing, simulated failures, ...) from *what* happens once it is found (fulfilling the promise of the
/// proxy future and invoking the user callback), whereby the latter is encapsulated in the `BuilderCallback`
/// which gets provided to `StartFind()`.
///
/// Lifecycle as driven by `ProxyBuilder::Build()`:
/// 1. `StartFind()` is invoked exactly once per strategy instance, handing over ownership of the `BuilderCallback`.
/// 2. Whenever a service instance got found, the strategy creates the concrete (backend-specific) proxy instance
///    and invokes the `BuilderCallback` with it.
/// 3. `StopFind()` is invoked once the returned `StopServiceDiscoveryAction` gets stopped or destroyed (e.g. together
///    with the future's owner) or once a stop got requested via the provided `stop_token`.
///
/// Additional requirements which are enforced by `ProxyBuilder` at compile time:
/// - A concrete strategy shall provide the type alias `BaseProxy` which equals `ProxySpec`.
/// - `StartFind()` and `StopFind()` shall be declared by the concrete strategy class itself, i.e. a mere inheritance
///   from a base class which declares them is not sufficient. `StopFind()` shall be declared `noexcept`.
///
/// @tparam ProxySpec Specification of the proxy (or proxies) the strategy finds. This is either a single proxy
///         interface or one of the tags `Variant<>` or `Multiple<>` wrapping proxy interface(s). It determines
///         the type of the `BuilderCallback` (cf. `ProxyBuilderBase<ProxySpec>::BuilderCallback`).
template <typename ProxySpec>
class FindServiceStrategy
{
  public:
    /// @brief Starts the service discovery.
    ///
    /// The strategy is free to either invoke `on_found` synchronously from within this method (e.g. immediate
    /// instantiation) or asynchronously, possibly from a different thread, once a service got found.
    ///
    /// The behaviour of `on_found` depends on `ProxySpec`:
    /// - Single, `Optional<>` or `Variant<>` proxy: The callback is a call-once callback, i.e. only its first
    ///   invocation has an effect. If it gets destroyed without having been invoked, the corresponding future
    ///   gets released with `score::concurrency::Error::kStopRequested`.
    /// - `Multiple<>`: The callback is reusable and shall be invoked once per found instance. The corresponding
    ///   future is already fulfilled with a container which gets populated by each invocation.
    ///
    /// @param on_found Callback which shall be invoked with the created proxy once a service got found. The strategy
    ///        takes ownership of it. In order to emulate an ongoing search which doesn't find anything, the strategy
    ///        has to keep it alive (at the latest until `StopFind()` gets invoked).
    ///
    /// @note `ProxyBuilder` invokes `StopFind()` only after this method returned.
    virtual void StartFind(std::unique_ptr<typename ProxyBuilderBase<ProxySpec>::BuilderCallback> on_found) = 0;

    /// @brief Stops the service discovery and releases the resources associated with it (e.g. a `BuilderCallback`
    ///        which is still held because no service got found).
    ///
    /// Implementations shall be thread-safe and idempotent since the method may be invoked repeatedly (e.g. once due
    /// to a stop request and again upon destruction of the service discovery), from a different thread while `on_found`
    /// is still executing, or without `StartFind()` having been invoked before (e.g. upon destruction of the strategy).
    virtual void StopFind() noexcept = 0;

    virtual ~FindServiceStrategy() = default;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_FIND_SERVICE_STRATEGY_H
