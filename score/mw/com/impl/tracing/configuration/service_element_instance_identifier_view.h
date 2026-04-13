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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SERVICE_ELEMENT_INSTANCE_IDENTIFIER_VIEW_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SERVICE_ELEMENT_INSTANCE_IDENTIFIER_VIEW_H

#include "score/mw/com/impl/tracing/configuration/hash_helper_utility.h"
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include <array>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

namespace score::mw
{
namespace log
{
class LogStream;
}
namespace com::impl::tracing
{

/// \brief Binding independent unique identifier of an instance of a service element (I.e. event, field, method) which
/// contains owning strings.
struct ServiceElementInstanceIdentifierView
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceElementIdentifierView service_element_identifier_view;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::string_view instance_specifier;
};

bool operator==(const ServiceElementInstanceIdentifierView& lhs,
                const ServiceElementInstanceIdentifierView& rhs) noexcept;

::score::mw::log::LogStream& operator<<(
    ::score::mw::log::LogStream& log_stream,
    const ServiceElementInstanceIdentifierView& service_element_instance_identifier_view);

}  // namespace com::impl::tracing
}  // namespace score::mw

namespace std
{

template <>
// NOLINTBEGIN(score-struct-usage-compliance): STL defines hash as struct.
// Suppress "AUTOSAR C++14 A11-0-2" rule finding.
// This rule states: "A type defined as struct shall: (1) provide only public data members, (2)
// not provide any special member functions or methods, (3) not be a base of
// another struct or class, (4) not inherit from another struct or class.".
// Rationale: Point 2 of this rule is violated. Keeping struct for consistency with STL.

// Suppress "AUTOSAR C++14 M3-2-3" rule finding.
// This rule states: "A type, object, or function that is used in multiple translation units shall be declared in one
// and only one file."
// Rationale: This is a false positive. The struct is declared in only one file. The Coverity tool
// issues a violation due to template specialization in several places.

// coverity[autosar_cpp14_a11_0_2_violation]
// coverity[autosar_cpp14_m3_2_3_violation]
struct hash<score::mw::com::impl::tracing::ServiceElementInstanceIdentifierView>
// NOLINTEND(score-struct-usage-compliance): STL defines hash as struct.
{
    size_t operator()(const score::mw::com::impl::tracing::ServiceElementInstanceIdentifierView& value) const noexcept
    {
        const auto& service_element_identifier_view = value.service_element_identifier_view;
        const auto& instance_specifier = value.instance_specifier;

        // To prevent dynamic memory allocations, we copy the input ServiceElementIdentifier elements into a local
        // buffer and create a string view to the local buffer. We then calculate the hash of the string_view.
        constexpr std::size_t max_buffer_size{1024U};
        constexpr auto service_element_type_size =
            sizeof(static_cast<std::underlying_type_t<decltype(service_element_identifier_view.service_element_type)>>(
                service_element_identifier_view.service_element_type));

        const std::array<std::size_t, 4U> input_values{{service_element_identifier_view.service_type_name.size(),
                                                        service_element_identifier_view.service_element_name.size(),
                                                        service_element_type_size,
                                                        instance_specifier.size()}};
        const auto [input_value_size, data_overflow_error] = score::mw::com::impl::tracing::configuration::Accumulate(
            input_values.begin(),
            input_values.end(),
            // std::size_t{0U} triggers false positive A5-2-2 violation, see Ticket-161711
            std::size_t{});
        if (data_overflow_error || (input_value_size > max_buffer_size))
        {
            score::mw::log::LogFatal("lola") << "ServiceElementInstanceIdentifierView data strings (service_type_name, "
                                                "service_element_name and instance_specifier) are too long: size"
                                             << input_value_size << "should be less than"
                                             << (max_buffer_size - service_element_type_size) << ". Terminating.";
            std::terminate();
        }

        std::array<char, max_buffer_size> local_buffer{};

        auto first_free_pos = local_buffer.begin();

        first_free_pos = std::copy(service_element_identifier_view.service_type_name.begin(),
                                   service_element_identifier_view.service_type_name.end(),
                                   first_free_pos);
        first_free_pos = std::copy(service_element_identifier_view.service_element_name.begin(),
                                   service_element_identifier_view.service_element_name.end(),
                                   first_free_pos);
        first_free_pos = std::copy(instance_specifier.begin(), instance_specifier.end(), first_free_pos);

        static_assert(service_element_type_size == sizeof(char), "service_element_type is not of size char");
        *first_free_pos = static_cast<char>(service_element_identifier_view.service_element_type);

        std::string_view local_string_buffer{local_buffer.data(), input_value_size};
        return std::hash<std::string_view>{}(local_string_buffer);
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SERVICE_ELEMENT_INSTANCE_IDENTIFIER_VIEW_H
