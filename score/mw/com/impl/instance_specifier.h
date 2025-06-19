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
#ifndef SCORE_MW_COM_IMPL_INSTANCE_SPECIFIER_H
#define SCORE_MW_COM_IMPL_INSTANCE_SPECIFIER_H

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <string>
#include <string_view>

namespace score::mw::com::impl
{

class InstanceSpecifier
{

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // friend is used here to give an STL function access to the internals of the class. This is in spirit the same type
    // of deviation as the exception provided in the rule "It is allowed to declare comparison operators as
    // friend functions."
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend std::hash<InstanceSpecifier>;

  public:
    static score::Result<InstanceSpecifier> Create(const std::string_view shortname_path) noexcept;

    std::string_view ToString() const noexcept;

  private:
    explicit InstanceSpecifier(const std::string_view shortname_path) noexcept;

    std::string instance_specifier_string_;
};

bool operator==(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept;
bool operator==(const InstanceSpecifier& lhs, const std::string_view& rhs) noexcept;
bool operator==(const std::string_view& lhs, const InstanceSpecifier& rhs) noexcept;

bool operator!=(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept;
bool operator!=(const InstanceSpecifier& lhs, const std::string_view& rhs) noexcept;
bool operator!=(const std::string_view& lhs, const InstanceSpecifier& rhs) noexcept;

bool operator<(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept;

::score::mw::log::LogStream& operator<<(::score::mw::log::LogStream& log_stream,
                                      const InstanceSpecifier& instance_specifier);

}  // namespace score::mw::com::impl

namespace std
{

/// \brief InstanceSpecifier is used as a key for maps, so we need a hash func for it.
template <>
class hash<score::mw::com::impl::InstanceSpecifier>
{
  public:
    std::size_t operator()(const score::mw::com::impl::InstanceSpecifier& instance_specifier) const noexcept
    {
        return std::hash<std::string>{}(instance_specifier.instance_specifier_string_);
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_INSTANCE_SPECIFIER_H
