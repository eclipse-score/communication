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
#ifndef SCORE_LIB_MESSAGE_PASSING_NON_ALLOCATING_FUTURE_NON_ALLOCATING_FUTURE_H
#define SCORE_LIB_MESSAGE_PASSING_NON_ALLOCATING_FUTURE_NON_ALLOCATING_FUTURE_H

#include <mutex>
#include <utility>
#include <variant>

namespace score::message_passing::detail
{

template <typename Lockable, typename CV, typename Value>
class NonAllocatingFuture
{
  public:
    NonAllocatingFuture(Lockable& mutex, CV& cv, Value& value) noexcept
        : mutex_(mutex), cv_(cv), value_(value), ready_(false)
    {
    }

    NonAllocatingFuture(const NonAllocatingFuture&) = delete;
    NonAllocatingFuture(NonAllocatingFuture&&) = delete;
    NonAllocatingFuture& operator=(const NonAllocatingFuture&) = delete;
    NonAllocatingFuture& operator=(NonAllocatingFuture&&) = delete;

    ~NonAllocatingFuture() noexcept = default;

    void UpdateValueMarkReady(const Value& value) noexcept
    {
        GetValueForUpdate() = value;
        MarkReady();
    }
    void UpdateValueMarkReady(Value&& value) noexcept
    {
        GetValueForUpdate() = std::move(value);
        MarkReady();
    }

    Value& GetValueForUpdate() const noexcept
    {
        return value_;
    }
    const Value& GetValue() const noexcept
    {
        return value_;
    }

    void MarkReady() noexcept
    {
        std::lock_guard lock{mutex_};
        ready_ = true;
        cv_.notify_all();
    }

    void Wait() noexcept
    {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this]() {
            return ready_;
        });
    }

  private:
    Lockable& mutex_;
    CV& cv_;
    Value& value_;
    bool ready_;
};

template <typename Lockable, typename CV>
class NonAllocatingFuture<Lockable, CV, void> : NonAllocatingFuture<Lockable, CV, std::monostate>
{
  public:
    NonAllocatingFuture(Lockable& mutex, CV& cv) noexcept
        : NonAllocatingFuture<Lockable, CV, std::monostate>(mutex, cv, blank_), blank_{}
    {
    }
    using NonAllocatingFuture<Lockable, CV, std::monostate>::MarkReady;
    using NonAllocatingFuture<Lockable, CV, std::monostate>::Wait;

  private:
    std::monostate blank_;
};

template <typename Lockable, typename CV>
NonAllocatingFuture(Lockable& mutex, CV& cv) -> NonAllocatingFuture<Lockable, CV, void>;

}  // namespace score::message_passing::detail

#endif  // SCORE_LIB_MESSAGE_PASSING_NON_ALLOCATING_FUTURE_NON_ALLOCATING_FUTURE_H
