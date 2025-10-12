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
#include <type_traits>

namespace score::mw::com::impl
{
/** \api
 * \brief Identifier for an application port. Maps design to deployment.
 * \public
 * \requirement SWS_CM_00350
 */
class InstanceSpecifier
{

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // friend is used here to give an STL function access to the internals of the class. This is in spirit the same type
    // of deviation as the exception provided in the rule "It is allowed to declare comparison operators as
    // friend functions."
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend std::hash<InstanceSpecifier>;

  public:
    /**
     * \api
     * \brief Create an InstanceSpecifier from a shortname path.
     * \param shortname_path The shortname path to create the InstanceSpecifier from.
     * \return A Result containing the created InstanceSpecifier or an error.
     * \public
     */
    static score::Result<InstanceSpecifier> Create(std::string&& shortname_path) noexcept;

    /**
     * \brief Create an InstanceSpecifier from various string-like types (deprecated template version).
     * \details Converting former static score::Result<InstanceSpecifier> Create(const std::string_view shortname_path)
     *          to this method template fixes the potential "ambiguity", when calling Create() with a string literal.
     *          With the former (non-template) signature, there would be an ambiguity between both Create() overloads,
     *          one with std::string&& and one with std::string_view. But replacing the std::string_view overload
     *          with the method-template solves the ambiguity, because Template Create(T&&) Matches, as T = const
     *          char(&)[]
     * \param shortname_path The shortname path to create the InstanceSpecifier from (convertible to std::string_view).
     * \return A Result containing the created InstanceSpecifier or an error.
     * \deprecated Use Create(std::string&&) instead for better performance.
     */
    template <typename T>
    [[deprecated(
        "Please use Create(std::string&&) instead for better performance"
        "The API will be removed from November 2025. A ticket is already created to track the removal: Ticket-214582")]]
    static score::Result<InstanceSpecifier> Create(T&& shortname_path) noexcept
    {
        static_assert(std::is_same_v<std::decay_t<T>, std::string_view> ||
                          std::is_constructible_v<std::string_view, std::decay_t<T>>,
                      "Parameter must be convertible to std::string_view");
        return Create(std::string{shortname_path});
    }

    /**
     * \api
     * \brief Convert the InstanceSpecifier to a string representation.
     * \return A string representation of the InstanceSpecifier.
     * \public
     */
    std::string_view ToString() const noexcept;

  private:
    explicit InstanceSpecifier(std::string&& shortname_path) noexcept;

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
