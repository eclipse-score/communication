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

#ifndef SCORE_MW_SERVICE_MULTI_INSTANCE_HOLDER_H
#define SCORE_MW_SERVICE_MULTI_INSTANCE_HOLDER_H

#include "score/mw/service/single_instance_holder.h"

#include "score/concurrency/condition_variable.h"

#include <score/callback.hpp>
#include <score/stop_token.hpp>
#include <score/utility.hpp>

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace score::mw::service
{

/// @brief A thread safe container that stores found instances and enables to visit them
/// @tparam InstanceType the (base) type of the instance which shall be stored
/// @tparam InstanceHolder the holder type of the single instance that shall be stored
/// @tparam InstanceHolderCreator helper type for constructing a single instance within a holder
template <typename InstanceType,
          typename InstanceHolder = SingleInstanceHolder<InstanceType>,
          typename InstanceHolderCreator = SingleInstanceHolderCreator<InstanceType>>
class MultiInstanceHolder
{
  public:
    explicit MultiInstanceHolder() : data_{std::make_shared<Data>()} {}

    constexpr MultiInstanceHolder(MultiInstanceHolder&&) noexcept = default;
    constexpr MultiInstanceHolder(const MultiInstanceHolder&) noexcept = default;
    constexpr MultiInstanceHolder& operator=(MultiInstanceHolder&&) & noexcept = default;
    constexpr MultiInstanceHolder& operator=(const MultiInstanceHolder&) & noexcept = default;

    ~MultiInstanceHolder() noexcept = default;

    /// @brief Invokes `visitor` for all found instances (thread-safe)
    ///
    /// @note Guarantees that no instances are added while visited
    ///
    /// @param visitor any invocable
    template <typename Visitor>
    void Visit(Visitor visitor) const
    {
        static_assert(std::is_invocable_v<Visitor, InstanceType&>,
                      "Visitor `visitor` must be invocable with `InstanceType&` as parameter type!");
        // This lock guard ensures the mutex is locked for the duration of this scope and is not dead code.
        // coverity[autosar_cpp14_m0_1_3_violation] see above justification
        // coverity[autosar_cpp14_m0_1_9_violation] see above justification
        const std::lock_guard<std::mutex> lock{data_->mutex};
        for (auto& instance : data_->instances)
        {
            std::invoke(visitor, *instance);
        }
    }

    /// @brief Waits until the user-provided predicate for checking whether certain instances got found returns true
    ///
    /// @param stop_waiting a predicate which shall return true in case the wait operation is considered to be complete
    /// @param max_wait_time an object of type std::chrono::duration representing the maximum time to spend waiting
    /// @param stop_token an score::cpp::stop_token to be used for cancelling the wait operation upon stop request
    ///
    /// @return true in case stop_waiting() yielded true, false otherwise (i.e. upon interrupt or timeout)
    template <typename Predicate>
    bool WaitUntilInstancesGotFound(Predicate stop_waiting,
                                    const std::chrono::nanoseconds max_wait_time,
                                    const score::cpp::stop_token& stop_token) const
    {
        static_assert(std::is_invocable_r_v<bool, Predicate, const std::vector<InstanceHolder>&>,
                      "Predicate `stop_waiting` must be invocable with `const std::vector<InstanceHolder>&` "
                      "as parameter type and return `bool`!");
        std::unique_lock<std::mutex> lock{data_->mutex};
        return data_->cv.wait_for(
            lock, stop_token, max_wait_time, [this, stop_waiting{std::move(stop_waiting)}]() -> bool {
                const std::vector<InstanceHolder>& instances = data_->instances;
                return std::invoke(stop_waiting, instances);
            });
    }

    /// @brief Add a found instance to the MultiInstanceHolder
    ///
    /// @note Might be blocked, if instances are currently visited.
    void FoundInstance(InstanceHolder instance)
    {
        // This lock guard ensures the mutex is locked for the duration of this scope and is not dead code.
        // coverity[autosar_cpp14_m0_1_3_violation] see above justification
        // coverity[autosar_cpp14_m0_1_9_violation] see above justification
        const std::lock_guard<std::mutex> lock{data_->mutex};
        score::cpp::ignore = data_->instances.emplace_back(std::move(instance));
        data_->cv.notify_all();
    }

    /// @brief Returns the number of contained instances
    /// @return the number of contained instances
    std::size_t NumInstances() const noexcept
    {
        // This lock guard ensures the mutex is locked for the duration of this scope and is not dead code.
        // coverity[autosar_cpp14_m0_1_3_violation] see above justification
        // coverity[autosar_cpp14_m0_1_9_violation] see above justification
        const std::lock_guard<std::mutex> lock{data_->mutex};
        return data_->instances.size();
    }

  private:
    struct Data
    {
        // coverity[autosar_cpp14_m11_0_1_violation] No harm, having public members in private struct
        std::mutex mutex{};
        // be reminded that std::list guarantees valid iterators, even in case other elements are removed
        // coverity[autosar_cpp14_m11_0_1_violation] No harm, having public members in private struct
        std::vector<InstanceHolder> instances{};
        // coverity[autosar_cpp14_m11_0_1_violation] No harm, having public members in private struct
        score::concurrency::InterruptibleConditionalVariable cv{};
    };
    std::shared_ptr<Data> data_;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_MULTI_INSTANCE_HOLDER_H
