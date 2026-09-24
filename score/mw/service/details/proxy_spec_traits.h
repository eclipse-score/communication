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

#ifndef SCORE_MW_SERVICE_DETAILS_PROXY_SPEC_TRAITS_H
#define SCORE_MW_SERVICE_DETAILS_PROXY_SPEC_TRAITS_H

#include "score/mw/service/multi_instance_holder.h"
#include "score/mw/service/proxy_data.h"
#include "score/mw/service/proxy_future.h"

#include "score/concurrency/future/error.h"
#include "score/concurrency/future/interruptible_promise.h"

#include "score/assert.hpp"
#include "score/overload.hpp"
#include "score/stop_token.hpp"
#include "score/utility.hpp"
#include <score/callback.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace score::mw::service
{

/// @brief Tag to identify that multiple instances of a service shall be found
template <typename ProxyBase>
struct Multiple
{
};

/// @brief Tag to identify that a service instance is optional
template <typename ProxyBase>
struct Optional
{
};

/// @brief Tag to identify that either of the service instances is required
template <typename... ProxyBase>
struct Variant
{
};

/// @brief The result type that can be returned by a ProxySpecTraits<>::InitCallback (which is provided by the user in
/// ProxyNeeds::WithOnServiceFound())
///
/// If kSuccess is returned, the Proxy will be created and provided to the user in the ResolvedType specified in
/// ProxySpecTraits. If kError is returned, The ResolvedType will contain the following:
///     Mandatory proxy: SingleInstanceHolder contains nullptr.
///     Optional proxy: Future contains an error.
///     Multiple proxies: MultiInstanceHolder does not contain a proxy.
///     Variant proxy: Currently, a default-constructed variant of the user will be returned which will fail to build
///     since it's not default constructible. This will need to be revised in the future.
enum class InitCallbackResult : bool
{
    kSuccess = true,
    kError = false
};

// Forward declaration required due to otherwise cyclic dependency, the class is declared only here.
// coverity[autosar_cpp14_m3_2_3_violation] see above justification
template <typename T>
class ProxyBuilderBase;

namespace details
{

template <typename ContainerType>
struct BuilderReturnType
{
    ProxyFuture<ContainerType> proxy_future;
    std::unique_ptr<StopServiceDiscoveryAction> service_discovery;
};

template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class MandatoryBuild
{
  public:
    using IsMandatory = std::true_type;
    using BuilderType = ProxyBuilderBase<ProxySpec>;

    constexpr MandatoryBuild() noexcept = default;
    ~MandatoryBuild() noexcept = default;

    MandatoryBuild(MandatoryBuild&&) = delete;
    MandatoryBuild(const MandatoryBuild&) = delete;
    MandatoryBuild& operator=(MandatoryBuild&&) = delete;
    MandatoryBuild& operator=(const MandatoryBuild&) = delete;

    [[nodiscard]] static auto Build(BuilderType& builder, std::optional<score::cpp::stop_token> stop_token)
    {
        return builder.Build(std::move(stop_token));
    }

    template <typename BuilderResult>
    [[nodiscard]] static auto Get(BuilderResult builder_result, score::cpp::stop_token stop_token)
    {
        auto proxy_future = std::move(builder_result.proxy_future);

        auto actual_result = proxy_future.Get(stop_token);
        if (actual_result.has_value())
        {
            return *std::move(actual_result);
        }
        if ((actual_result.error() == score::concurrency::Error::kStopRequested) ||
            (actual_result.error() == score::concurrency::Error::kUnset))
        {
            return std::decay_t<decltype(actual_result.value())>{};
        }

        // False positive, the assertion is reachable in error cases not covered by the preceding returns.
        // coverity[autosar_cpp14_m0_1_1_violation : FALSE] see above justification
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Unexpected `concurrency::Error` got received from `builder`'s future!");
    }

    template <typename BuilderResult>
    static auto Get(BuilderResult) = delete;
};

template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class OptionalBuild
{
  public:
    using IsMandatory = std::false_type;
    using BuilderType = ProxyBuilderBase<ProxySpec>;

    constexpr OptionalBuild() noexcept = default;
    ~OptionalBuild() noexcept = default;

    OptionalBuild(OptionalBuild&&) = delete;
    OptionalBuild(const OptionalBuild&) = delete;
    OptionalBuild& operator=(OptionalBuild&&) = delete;
    OptionalBuild& operator=(const OptionalBuild&) = delete;

    [[nodiscard]] static auto Build(BuilderType& builder, std::optional<score::cpp::stop_token> stop_token)
    {
        return builder.Build(std::move(stop_token));
    }

    template <typename BuilderResult>
    // The NonBlockingBuild class is a template class, each translation unit will instantiate the template class
    // separately, which does not violate the One Definition Rule.
    // coverity[autosar_cpp14_m3_2_2_violation] see above justification
    [[nodiscard]] static auto Get(BuilderResult builder_result, score::cpp::stop_token stop_token = {})
    {
        score::cpp::ignore = stop_token;
        auto proxy_future = std::move(builder_result.proxy_future);
        auto stop_action = std::move(builder_result.service_discovery);
        return OptionalProxyData<ProxySpec>{std::move(proxy_future), std::move(stop_action)};
    }
};

template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class MultiInstanceBuild
{
  public:
    using IsMandatory = std::false_type;
    using BuilderType = ProxyBuilderBase<Multiple<ProxySpec>>;

    constexpr MultiInstanceBuild() noexcept = default;
    ~MultiInstanceBuild() noexcept = default;

    MultiInstanceBuild(MultiInstanceBuild&&) = delete;
    MultiInstanceBuild(const MultiInstanceBuild&) = delete;
    MultiInstanceBuild& operator=(MultiInstanceBuild&&) = delete;
    MultiInstanceBuild& operator=(const MultiInstanceBuild&) = delete;

    [[nodiscard]] static auto Build(BuilderType& builder, std::optional<score::cpp::stop_token> stop_token)
    {
        return builder.Build(std::move(stop_token));
    }

    template <typename BuilderResult>
    // The NonBlockingBuild class is a template class, each translation unit will instantiate the template class
    // separately, which does not violate the One Definition Rule.
    // coverity[autosar_cpp14_m3_2_2_violation] see above justification
    [[nodiscard]] static auto Get(BuilderResult builder_result, score::cpp::stop_token stop_token = {})
    {
        auto proxy_future = std::move(builder_result.proxy_future);
        auto stop_action = std::move(builder_result.service_discovery);
        auto result = proxy_future.Get(std::move(stop_token));
        if (result.has_value())
        {
            return MultipleProxyData<ProxySpec>{*std::move(result), std::move(stop_action)};
        }

        return MultipleProxyData<ProxySpec>{std::decay_t<decltype(result.value())>{}, std::move(stop_action)};
    }
};

// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class DeriveUserCallbackParameter
{
  public:
    template <typename ProxyType>
    // A reference is used here as this method is called from a lambda context where ownership is not transferred.
    // coverity[autosar_cpp14_a8_4_12_violation] see above justification
    constexpr static ProxyType& From(const SingleInstanceHolder<ProxyType>& single_holder)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(single_holder != nullptr);
        return *single_holder;
    }

    template <typename ProxyType>
    constexpr static ProxyType& From(std::optional<ProxyType>& optional_holder)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(optional_holder.has_value());
        return optional_holder.value();
    }

    template <typename... ProxyTypes>
    constexpr static std::variant<ProxyTypes*...> From(
        std::variant<SingleInstanceHolder<ProxyTypes>...>& variant_holder)
    {
        return Visit<ProxyTypes...>(variant_holder);
    }

    template <typename... ProxyTypes>
    constexpr static std::variant<ProxyTypes*...> From(std::variant<std::optional<ProxyTypes>...>& variant_holder)
    {
        return Visit<ProxyTypes...>(variant_holder);
    }

  private:
    template <typename... ProxyTypes, typename VariantType>
    constexpr static std::variant<ProxyTypes*...> Visit(VariantType& variant)
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(not(variant.valueless_by_exception()));
        return std::visit(
            [](auto& instance_holder) -> std::variant<ProxyTypes*...> {
                return {&DeriveUserCallbackParameter::From(instance_holder)};
            },
            variant);
    }
};

template <typename Trait>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class CallOnceProxyBuilderCallback
{
    static_assert(std::is_same_v<typename Trait::HolderType, typename Trait::ContainerType>, "");

  public:
    CallOnceProxyBuilderCallback(score::concurrency::InterruptiblePromise<typename Trait::ContainerType> promise,
                                 typename Trait::UserCallbackVariant callback)
        : promise_{std::move(promise)}, callback_{std::move(callback)}, once_flag_{}
    {
    }

    constexpr CallOnceProxyBuilderCallback& operator=(const CallOnceProxyBuilderCallback&) & = delete;
    constexpr CallOnceProxyBuilderCallback& operator=(CallOnceProxyBuilderCallback&&) & noexcept = default;
    constexpr CallOnceProxyBuilderCallback(CallOnceProxyBuilderCallback&&) noexcept = default;
    constexpr CallOnceProxyBuilderCallback(const CallOnceProxyBuilderCallback&) = delete;

    ~CallOnceProxyBuilderCallback() noexcept
    {
        // Notify potentially waiting futures by releasing the promise with the correct error code.
        // NOTE: In case the promise is already satisfied (i.e. got a value set), this call has no effect.
        score::cpp::ignore = promise_.SetError(score::concurrency::Error::kStopRequested);
    }

    void operator()(typename Trait::HolderType proxy_instance)
    {
        using HolderType = typename Trait::HolderType;
        std::call_once(
            once_flag_,
            [this](HolderType proxy) {
                auto visitor = score::cpp::overload(
                    [&proxy](const typename Trait::UserCallback& callback) noexcept -> InitCallbackResult {
                        callback(DeriveUserCallbackParameter::From(proxy));
                        return InitCallbackResult::kSuccess;
                    },
                    [&proxy](const typename Trait::InitCallback& callback) noexcept -> InitCallbackResult {
                        return callback(DeriveUserCallbackParameter::From(proxy));
                    },
                    [](std::monostate) noexcept -> InitCallbackResult {
                        return InitCallbackResult::kSuccess;
                    });
                auto callback_result = std::visit(visitor, callback_);
                if (callback_result == InitCallbackResult::kSuccess)
                {
                    // We ignore `SetValue()`'s return value here since it's fine if the promise would
                    // already contain an error. Because in such case, waiting users would have already
                    // gotten notified and do no longer require the value we attempt to set here.
                    score::cpp::ignore = promise_.SetValue(std::move(proxy));
                }
                else
                {
                    score::cpp::ignore = promise_.SetError(score::concurrency::Error::kUnset);
                }
            },
            std::move(proxy_instance));
    }

  private:
    score::concurrency::InterruptiblePromise<typename Trait::HolderType> promise_;
    typename Trait::UserCallbackVariant callback_;
    std::once_flag once_flag_;
};

template <typename Trait>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class ReusableProxyBuilderCallback
{
  public:
    ReusableProxyBuilderCallback(score::concurrency::InterruptiblePromise<typename Trait::ContainerType> promise,
                                 typename Trait::UserCallbackVariant callback)
        : proxy_instances_{}, callback_{std::move(callback)}
    {
        // We ignore `SetValue()`'s return value here since it's fine if the promise would
        // already contain an error. Because in such case, waiting users would have already
        // gotten notified and do no longer require the value we attempt to set here.
        score::cpp::ignore = promise.SetValue(proxy_instances_);
    }

    void operator()(typename Trait::HolderType proxy_instance)
    {
        auto visitor = score::cpp::overload(
            [&proxy_instance](const typename Trait::UserCallback& callback) noexcept -> InitCallbackResult {
                callback(DeriveUserCallbackParameter::From(proxy_instance));
                return InitCallbackResult::kSuccess;
            },
            [&proxy_instance](const typename Trait::InitCallback& callback) noexcept -> InitCallbackResult {
                return callback(DeriveUserCallbackParameter::From(proxy_instance));
            },
            [](std::monostate) noexcept -> InitCallbackResult {
                return InitCallbackResult::kSuccess;
            });
        auto callback_result = std::visit(visitor, callback_);
        if (callback_result == InitCallbackResult::kSuccess)
        {
            proxy_instances_.FoundInstance(std::move(proxy_instance));
        }
    }

  private:
    typename Trait::ContainerType proxy_instances_;
    typename Trait::UserCallbackVariant callback_;
};

constexpr std::size_t kInitCallbackSize = 256U;

// Base case - assuming mandatory proxy
template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class ProxySpecTraits : public MandatoryBuild<ProxySpec>
{
  public:
    using ProxyType = ProxySpec;
    using HolderType = SingleInstanceHolder<ProxyType>;
    using ContainerType = HolderType;
    using BuilderReturn = BuilderReturnType<ContainerType>;
    using ResolvedType = ContainerType;

    using UserCallback = score::cpp::callback<void(ProxyType&)>;
    using InitCallback = score::cpp::callback<InitCallbackResult(ProxyType&), kInitCallbackSize>;

    using NoUserCallback = std::monostate;
    using UserCallbackVariant = std::variant<NoUserCallback, UserCallback, InitCallback>;

    using BuilderCallback = CallOnceProxyBuilderCallback<ProxySpecTraits<ProxySpec>>;

    using CanBeUsedWithinMultiple = std::true_type;
    using CanBeUsedWithinOptional = std::true_type;
    using CanBeUsedWithinVariant = std::true_type;
};

// Optional specialization - resolving further to either base case (but not mandatory) or alternative
template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class ProxySpecTraits<Optional<ProxySpec>> : public OptionalBuild<ProxySpec>
{
    static_assert(ProxySpecTraits<ProxySpec>::CanBeUsedWithinOptional::value);

  public:
    using ProxyType = typename ProxySpecTraits<ProxySpec>::ProxyType;
    using HolderType = typename ProxySpecTraits<ProxySpec>::HolderType;
    using ContainerType = typename ProxySpecTraits<ProxySpec>::ContainerType;
    using BuilderReturn = BuilderReturnType<ContainerType>;
    using ResolvedType = OptionalProxyData<ProxySpec>;

    using UserCallback = typename ProxySpecTraits<ProxySpec>::UserCallback;
    using InitCallback = typename ProxySpecTraits<ProxySpec>::InitCallback;

    using NoUserCallback = typename ProxySpecTraits<ProxySpec>::NoUserCallback;
    using UserCallbackVariant = typename ProxySpecTraits<ProxySpec>::UserCallbackVariant;

    using BuilderCallback = typename ProxySpecTraits<ProxySpec>::BuilderCallback;

    using CanBeUsedWithinMultiple = std::false_type;
    using CanBeUsedWithinOptional = std::false_type;
    using CanBeUsedWithinVariant = std::false_type;
};

// Multiple specialization - resolving further to either base case (but not mandatory) or alternative
template <typename ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class ProxySpecTraits<Multiple<ProxySpec>> : public MultiInstanceBuild<ProxySpec>
{
    static_assert(ProxySpecTraits<ProxySpec>::CanBeUsedWithinMultiple::value);

  public:
    using ProxyType = typename ProxySpecTraits<ProxySpec>::ProxyType;
    using HolderType = typename ProxySpecTraits<ProxySpec>::HolderType;
    using ContainerType = MultiInstanceHolder<ProxyType, HolderType>;
    using BuilderReturn = BuilderReturnType<ContainerType>;
    using ResolvedType = MultipleProxyData<ProxySpec>;

    using UserCallback = typename ProxySpecTraits<ProxySpec>::UserCallback;
    using InitCallback = typename ProxySpecTraits<ProxySpec>::InitCallback;

    using NoUserCallback = typename ProxySpecTraits<ProxySpec>::NoUserCallback;
    using UserCallbackVariant = typename ProxySpecTraits<ProxySpec>::UserCallbackVariant;

    using BuilderCallback = ReusableProxyBuilderCallback<ProxySpecTraits<Multiple<ProxySpec>>>;

    using CanBeUsedWithinMultiple = std::false_type;
    using CanBeUsedWithinOptional = std::false_type;
    using CanBeUsedWithinVariant = std::false_type;
};

// Alternative specialization - assuming mandatory
template <typename... ProxySpec>
// coverity[autosar_cpp14_m3_2_3_violation] false positive, template class is declared only here and nowhere else
class ProxySpecTraits<Variant<ProxySpec...>> : public MandatoryBuild<Variant<ProxySpec...>>
{
    static_assert((ProxySpecTraits<ProxySpec>::CanBeUsedWithinVariant::value && ...));

  public:
    using ProxyType = std::false_type;  // serves as dummy typedef
    using HolderType = std::variant<SingleInstanceHolder<ProxySpec>...>;
    using ContainerType = HolderType;
    using BuilderReturn = BuilderReturnType<ContainerType>;
    using ResolvedType = ContainerType;

    using UserCallback = score::cpp::callback<void(std::variant<ProxySpec*...>)>;
    using InitCallback = score::cpp::callback<InitCallbackResult(std::variant<ProxySpec*...>), kInitCallbackSize>;

    using NoUserCallback = std::monostate;
    using UserCallbackVariant = std::variant<NoUserCallback, UserCallback, InitCallback>;

    using BuilderCallback = CallOnceProxyBuilderCallback<ProxySpecTraits<Variant<ProxySpec...>>>;

    using CanBeUsedWithinMultiple = std::false_type;  // requires some future work to be enabled
    using CanBeUsedWithinOptional = std::true_type;
    using CanBeUsedWithinVariant = std::false_type;
};

}  // namespace details
}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_DETAILS_PROXY_SPEC_TRAITS_H
