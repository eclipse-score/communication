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
#include "score/mw/com/impl/instance_specifier.h"

#include "score/mw/com/impl/com_error.h"

#include <regex>

namespace score::mw::com::impl
{

namespace
{

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate()
// function shall not be called implicitly". std::regex_search() will not throw exception
// as characters passed are pre checked for invalid data.
// coverity[autosar_cpp14_a15_5_3_violation]
bool IsShortNameValid(const score::cpp::string_view shortname) noexcept
{
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized". std::regex does not have a constexpr constructor and hence cannot be constexpr.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static std::regex check_characters_regex("^[A-Za-z_/][A-Za-z_/0-9]*", std::regex_constants::basic);
    // coverity[autosar_cpp14_a3_3_2_violation]
    static std::regex check_trailing_slash_regex("/$", std::regex_constants::basic);
    // coverity[autosar_cpp14_a3_3_2_violation]
    static std::regex check_duplicate_slashes_regex("/{2,}", std::regex_constants::extended);

    const bool all_characters_valid = std::regex_match(shortname.begin(), shortname.end(), check_characters_regex);
    const bool duplicate_slashes_found =
        std::regex_search(shortname.begin(), shortname.end(), check_duplicate_slashes_regex);
    const bool trailing_slash_found = std::regex_search(shortname.begin(), shortname.end(), check_trailing_slash_regex);

    return (((all_characters_valid) && (!duplicate_slashes_found)) && (!trailing_slash_found));
}

}  // namespace

score::Result<InstanceSpecifier> InstanceSpecifier::Create(const score::cpp::string_view shortname_path) noexcept
{
    if (!IsShortNameValid(shortname_path))
    {
        score::mw::log::LogWarn("lola") << "Shortname" << shortname_path
                                      << "does not adhere to shortname naming requirements.";
        return MakeUnexpected(ComErrc::kInvalidMetaModelShortname);
    }

    const InstanceSpecifier instance_specifier{shortname_path};
    return instance_specifier;
}

std::string_view InstanceSpecifier::ToString() const noexcept
{
    return instance_specifier_string_;
}

InstanceSpecifier::InstanceSpecifier(const score::cpp::string_view shortname_path) noexcept
    : instance_specifier_string_{shortname_path.data(), shortname_path.size()}
{
}

auto operator==(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept -> bool
{
    return lhs.ToString() == rhs.ToString();
}

// Suppress "AUTOSAR C++14 A13-5-5", The rule states: "Comparison operators shall be non-member functions with
// identical parameter types and noexcept.". There is no functional reason behind, we require comparing
// `InstanceSpecifier&` and `score::cpp::string_view&`.
// coverity[autosar_cpp14_a13_5_5_violation]
bool operator==(const InstanceSpecifier& lhs, const score::cpp::string_view& rhs) noexcept
{
    return lhs.ToString() == std::string_view{rhs.data(), rhs.size()};
}

// coverity[autosar_cpp14_a13_5_5_violation]
bool operator==(const score::cpp::string_view& lhs, const InstanceSpecifier& rhs) noexcept
{
    return std::string_view{lhs.data(), lhs.size()} == rhs.ToString();
}

auto operator!=(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

// coverity[autosar_cpp14_a13_5_5_violation]
bool operator!=(const InstanceSpecifier& lhs, const score::cpp::string_view& rhs) noexcept
{
    return !(lhs == rhs);
}

// coverity[autosar_cpp14_a13_5_5_violation]
bool operator!=(const score::cpp::string_view& lhs, const InstanceSpecifier& rhs) noexcept
{
    return !(lhs == rhs);
}

auto operator<(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept -> bool
{
    return lhs.ToString() < rhs.ToString();
}

// Suppress "AUTOSAR C++14 A13-2-2", The rule states: "A binary arithmetic operator and a bitwise operator shall return
// a “prvalue”." The code with '<<' is not a left shift operator but an overload for logging the respective types. code
// analysis tools tend to assume otherwise hence a false positive.
// coverity[autosar_cpp14_a13_2_2_violation : FALSE]
auto operator<<(::score::mw::log::LogStream& log_stream, const InstanceSpecifier& instance_specifier)
    -> ::score::mw::log::LogStream&
{
    log_stream << instance_specifier.ToString();
    return log_stream;
}

}  // namespace score::mw::com::impl
