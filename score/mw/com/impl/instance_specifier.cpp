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
    if (shortname.empty() || shortname.back() == '/')
    {
        return false;
    }

    // Validate first character
    const char first_char = shortname[0];
    if (!((static_cast<bool>(std::isalpha(first_char)) || first_char == '_') || first_char == '/'))
    {
        return false;
    }
    // Single pass validation
    for (std::size_t char_index = 1; char_index < shortname.size(); ++char_index)
    {
        const char current_char = shortname[char_index];
        if (!((static_cast<bool>(std::isalnum(current_char)) || current_char == '_') || current_char == '/'))
        {
            return false;
        }
        if (current_char == '/' && shortname[char_index - 1] == '/')
        {
            return false;
        }
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
