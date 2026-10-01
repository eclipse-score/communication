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

#ifndef SCORE_MW_SERVICE_DETAILS_PROXY_BUILDER_H
#define SCORE_MW_SERVICE_DETAILS_PROXY_BUILDER_H

#include "score/mw/service/proxy_builder_base.h"

#include "score/concurrency/future/interruptible_promise.h"

#include <score/overload.hpp>
#include <score/utility.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace score::mw::service::details
{

// Helper methods for static_asserts of ProxyBuilder (fake C++20 concepts)
template <typename T>
using HasMemberStopFind = typename std::is_same<decltype(&T::StopFind), void (T::*)()>;
template <typename T>
using HasMemberStopFindNoexcept = typename std::is_same<decltype(&T::StopFind), void (T::*)() noexcept>;

template <typename T>
// coverity[autosar_cpp14_a2_10_4_violation: FALSE] Just a single definition
constexpr bool kHasMemberStopFind = HasMemberStopFind<T>::value;
template <typename T>
constexpr bool kHasMemberStopFindNoexcept = HasMemberStopFindNoexcept<T>::value;

template <typename T>
using HasMemberFind =
    typename std::is_same<decltype(&T::Find),
                          void (T::*)(
                              std::unique_ptr<typename ProxyBuilderBase<typename T::BaseProxy>::BuilderCallback>)>;
template <typename T>
using HasMemberFindNoexcept = typename std::is_same<
    decltype(&T::Find),
    void (T::*)(std::unique_ptr<typename ProxyBuilderBase<typename T::BaseProxy>::BuilderCallback>) noexcept>;

template <typename T>
constexpr bool kHasMemberFind = HasMemberFind<T>::value || HasMemberFindNoexcept<T>::value;

/// \brief Starts service discovery for a proxy and provides a future for its instantiation
///
/// Named requirements towards `Strategy`:
/// - Must have type-def defining which Interface will be constructed (BaseProxy)
/// - Must have method Find(std::unique_ptr<BuilderCallback>)
/// - Must have method StopFind() noexcept
/// \tparam Strategy
template <typename Strategy>
class ProxyBuilder final : public ProxyBuilderBase<typename Strategy::BaseProxy>
{
  public:
    using Trait = details::ProxySpecTraits<typename Strategy::BaseProxy>;

    explicit ProxyBuilder(std::unique_ptr<Strategy> strategy) noexcept
        : ProxyBuilderBase<typename Strategy::BaseProxy>(), strategy_{std::move(strategy)}
    {
        // Check whether template type `Strategy` implements all our requirements
        static_assert(kHasMemberFind<Strategy>,
                      "Strategy does not implement: "
                      "void Find(std::unique_ptr<typename ProxyBuilder<>::BuilderCallback> on_found)");

        // Rationale: This is a false positive because "if constexpr" is a valid statement since C++17.
        // coverity[autosar_cpp14_a7_1_8_violation] see above justification
        // coverity[autosar_cpp14_m6_4_1_violation] False positive, statement is a compound statement
        if constexpr (kHasMemberStopFind<Strategy>)
        {
            static_assert(kHasMemberStopFindNoexcept<Strategy>,
                          "Strategy's method StopFind() is required to be declared noexcept");
        }
        else
        {
            static_assert(kHasMemberStopFindNoexcept<Strategy>,
                          "Strategy does not implement: void StopFind() noexcept");
        }
    }

    // This is false positive. The underlying return types are the same.
    // coverity[autosar_cpp14_m3_9_1_violation] see above justification
    [[nodiscard]] auto Build(std::optional<score::cpp::stop_token> stop_token) ->
        typename details::ProxySpecTraits<typename Strategy::BaseProxy>::BuilderReturn override
    {
        score::concurrency::InterruptiblePromise<typename Trait::ContainerType> promise{};
        mw::service::ProxyFuture<typename Trait::ContainerType> future{promise.GetInterruptibleFuture().value()};

        auto on_service_found_user_callback = ExtractUserCallbackFrom(std::move(this->callback_));
        auto on_service_found_builder_callback =
            std::make_unique<typename ProxyBuilderBase<typename Strategy::BaseProxy>::BuilderCallback>(
                std::move(promise), std::move(on_service_found_user_callback));

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(strategy_ != nullptr);
        std::unique_ptr<Strategy> strategy;
        strategy.swap(strategy_);

        strategy->Find(std::move(on_service_found_builder_callback));

        // NOTE: The stop_callback must only be set up *after* the Strategy's `Find()` call returned since otherwise
        // `ActiveServiceDiscovery()` could get invoked before `Find()` and the service discovery would run indefinitely
        // then.
        class ActiveServiceDiscovery final : public StopServiceDiscoveryAction
        {
          public:
            ActiveServiceDiscovery(std::unique_ptr<Strategy> strategy,
                                   std::optional<score::cpp::stop_token> stop_token) noexcept
                : StopServiceDiscoveryAction{}, strategy_{std::move(strategy)}, stop_callback_{}
            {
                if (stop_token.has_value())
                {
                    score::cpp::ignore = stop_callback_.emplace(*std::move(stop_token), [this]() noexcept {
                        strategy_->StopFind();
                    });
                }
                else
                {
                    score::cpp::ignore = stop_token;  // dummy stmt to demonstrate this branch as covered
                }
            }

            ActiveServiceDiscovery(const ActiveServiceDiscovery&) = delete;
            ActiveServiceDiscovery& operator=(const ActiveServiceDiscovery&) = delete;
            ActiveServiceDiscovery(ActiveServiceDiscovery&&) = delete;
            ActiveServiceDiscovery& operator=(ActiveServiceDiscovery&&) = delete;

            ~ActiveServiceDiscovery() noexcept
            {
                Stop();
            }

            void Stop() noexcept override final
            {
                stop_callback_.reset();
                if (strategy_)
                {
                    strategy_->StopFind();
                    strategy_.reset();
                }
            }

          private:
            std::unique_ptr<Strategy> strategy_;
            std::optional<score::cpp::stop_callback> stop_callback_;
        };

        return typename Trait::BuilderReturn{
            std::move(future), std::make_unique<ActiveServiceDiscovery>(std::move(strategy), std::move(stop_token))};
    }

  private:
    /// @brief Converts a UserCallbackVariant which contains an empty UserCallback or InitCallback to NoUserCallback,
    /// otherwise returns the UserCallbackVariant untouched
    static typename Trait::UserCallbackVariant ExtractUserCallbackFrom(
        typename Trait::UserCallbackVariant&& callback_variant)
    {
        using UserCallbackVariant = typename Trait::UserCallbackVariant;
        return std::visit(score::cpp::overload(
                              [](typename Trait::NoUserCallback) noexcept -> UserCallbackVariant {
                                  return typename Trait::NoUserCallback{};
                              },
                              [](typename Trait::UserCallback user_callback) noexcept -> UserCallbackVariant {
                                  if (user_callback.empty())
                                  {
                                      return typename Trait::NoUserCallback{};
                                  }
                                  return user_callback;
                              },
                              [](typename Trait::InitCallback init_callback) noexcept -> UserCallbackVariant {
                                  if (init_callback.empty())
                                  {
                                      return typename Trait::NoUserCallback{};
                                  }
                                  return init_callback;
                              }),
                          std::move(callback_variant));
    }

    std::unique_ptr<Strategy> strategy_;
};

}  // namespace score::mw::service::details

#endif  // SCORE_MW_SERVICE_DETAILS_PROXY_BUILDER_H
