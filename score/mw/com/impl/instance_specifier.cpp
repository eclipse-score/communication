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

#include <cctype>

namespace score::mw::com::impl
{

namespace
{

/**
 * @brief Validates whether a shortname string adheres to the naming requirements.
 *
 * @details Validation Rules:
 * - Must not be empty
 * - First character must be: letter (a-z, A-Z), underscore (_), or forward slash (/)
 * - Subsequent characters must be: alphanumeric (a-z, A-Z, 0-9), underscore (_), or forward slash (/)
 * - Must not end with a forward slash (/)
 * - Must not contain consecutive forward slashes (//)
 *
 * @param shortname The shortname string to validate
 * @return true if the shortname is valid according to all rules, false otherwise
 */
bool IsShortNameValid(const std::string_view shortname) noexcept
{
    if (shortname.empty() || (shortname.back() == '/'))
    {
        return false;
    }

    auto validate_chars = [](auto it_begin, auto it_end, const bool first_char) -> bool {
        const auto found_invalid_chars = std::find_if_not(it_begin, it_end, [first_char](const auto curent_char) {
            const auto u_ch = static_cast<unsigned char>(curent_char);
            // Suppress "AUTOSAR C++14 M5-0-3" and  "AUTOSAR C++14 M5-0-4" rules, which state: "A cvalue expression
            // shall not be implicitly converted to a different underlying type." and "An implicit integral conversion
            // shall not change the signedness of the underlying type." respectively Rationale: This is tolerated as
            // static_cast from int to bool will not change the signedness and the type convertion is intended
            // coverity[autosar_cpp14_m5_0_3_violation]
            // coverity[autosar_cpp14_m5_0_4_violation]
            const auto is_alpha_or_num = first_char ? std::isalpha(u_ch) : std::isalnum(u_ch);
            // coverity[autosar_cpp14_a5_2_6_violation: FALSE] False positive: each operand is parenthesized
            return ((static_cast<bool>(is_alpha_or_num)) || (curent_char == '_') || (curent_char == '/'));
        });
        return found_invalid_chars == it_end;
    };
    // Validate first character
    if (!validate_chars(shortname.begin(), std::next(shortname.begin()), true))
    {
        return false;
    }
    // Single pass validation
    if (!validate_chars(std::next(shortname.begin()), shortname.end(), false))
    {
        return false;
    }

    constexpr auto invalid_char_seq = "//";
    if (shortname.find(invalid_char_seq, 0U) != std::string_view::npos)
    {
        return false;
    }
    return true;
}

}  // namespace

score::Result<InstanceSpecifier> InstanceSpecifier::Create(std::string&& shortname_path) noexcept
{
    if (!IsShortNameValid(shortname_path))
    {
        score::mw::log::LogWarn("lola") << "Shortname" << shortname_path
                                        << "does not adhere to shortname naming requirements.";
        return MakeUnexpected(ComErrc::kInvalidMetaModelShortname);
    }

    return InstanceSpecifier{std::move(shortname_path)};
}

std::string_view InstanceSpecifier::ToString() const noexcept
{
    return instance_specifier_string_;
}

InstanceSpecifier::InstanceSpecifier(std::string&& shortname_path) noexcept
    : instance_specifier_string_{std::move(shortname_path)}
{
}

auto operator==(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept -> bool
{
    return lhs.ToString() == rhs.ToString();
}

// Suppress "AUTOSAR C++14 A13-5-5", The rule states: "Comparison operators shall be non-member functions with
// identical parameter types and noexcept.". There is no functional reason behind, we require comparing
// `InstanceSpecifier&` and `std::string_view&`.
// coverity[autosar_cpp14_a13_5_5_violation]
bool operator==(const InstanceSpecifier& lhs, const std::string_view& rhs) noexcept
{
    return lhs.ToString() == rhs;
}

// coverity[autosar_cpp14_a13_5_5_violation]
bool operator==(const std::string_view& lhs, const InstanceSpecifier& rhs) noexcept
{
    return lhs == rhs.ToString();
}

auto operator!=(const InstanceSpecifier& lhs, const InstanceSpecifier& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

// coverity[autosar_cpp14_a13_5_5_violation]
bool operator!=(const InstanceSpecifier& lhs, const std::string_view& rhs) noexcept
{
    return !(lhs == rhs);
}

// coverity[autosar_cpp14_a13_5_5_violation]
bool operator!=(const std::string_view& lhs, const InstanceSpecifier& rhs) noexcept
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
