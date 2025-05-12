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
#ifndef SCORE_MW_COM_IMPL_FLAG_OWNER_H
#define SCORE_MW_COM_IMPL_FLAG_OWNER_H

namespace score::mw::com::impl
{

/// \brief Helper class which maintains a flag that has a single owner.
/// When an object of this class is move constructed or move assigned, the flag of the moved-from object will be
/// cleared.
class FlagOwner
{
  public:
    explicit FlagOwner() noexcept : FlagOwner{false} {}
    explicit FlagOwner(bool initial_value) noexcept : flag_{initial_value} {}

    ~FlagOwner() noexcept = default;

    FlagOwner(const FlagOwner&) = delete;
    FlagOwner& operator=(const FlagOwner&) & = delete;

    FlagOwner(FlagOwner&& other) noexcept : flag_{other.flag_}
    {
        // Suppress "AUTOSAR C++14 A18-9-2" rule findings. This rule stated: "Forwarding values to other functions shall
        // be done via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding
        // reference". We are not forwarding any value here, we clear the container.
        // coverity[autosar_cpp14_a18_9_2_violation]
        other.Clear();
    }

    FlagOwner& operator=(FlagOwner&& other) & noexcept
    {
        if (this != &other)
        {
            flag_ = other.flag_;
            // coverity[autosar_cpp14_a18_9_2_violation]
            other.Clear();
        }
        return *this;
    }

    void Set() noexcept
    {
        flag_ = true;
    }
    void Clear() noexcept
    {
        flag_ = false;
    }

    bool IsSet() const noexcept
    {
        return flag_;
    }

  private:
    bool flag_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_FLAG_OWNER_H
