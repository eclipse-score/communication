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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACE_POINT_KEY_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACE_POINT_KEY_H

#include "score/mw/com/impl/tracing/configuration/hash_helper_utility.h"
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include "score/mw/log/logging.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string_view>

namespace score::mw::com::impl::tracing
{

struct TracePointKey
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceElementIdentifierView service_element{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::uint8_t trace_point_type{};
};

bool operator==(const TracePointKey& lhs, const TracePointKey& rhs) noexcept;

}  // namespace score::mw::com::impl::tracing

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
struct hash<score::mw::com::impl::tracing::TracePointKey>
// NOLINTEND(score-struct-usage-compliance): STL defines hash as struct.
{
    size_t operator()(const score::mw::com::impl::tracing::TracePointKey& value) const noexcept
    {
        // To prevent dynamic memory allocations, we copy the input TracePointKey elements into a local buffer and
        // create a string view to the local buffer. We then calculate the hash of the string_view.
        constexpr std::size_t max_buffer_size{1024U};
        constexpr auto service_element_type_size =
            sizeof(static_cast<std::underlying_type_t<decltype(value.service_element.service_element_type)>>(
                value.service_element.service_element_type));

        const std::array<std::size_t, 4U> input_values{{value.service_element.service_type_name.size(),
                                                        value.service_element.service_element_name.size(),
                                                        service_element_type_size,
                                                        sizeof(value.trace_point_type)}};
        const auto [input_value_size, data_overflow_error] = score::mw::com::impl::tracing::configuration::Accumulate(
            input_values.begin(),
            input_values.end(),
            // std::size_t{0U} triggers false positive A5-2-2 violation, see Ticket-161711
            std::size_t{});
        if (data_overflow_error || (input_value_size > max_buffer_size))
        {
            score::mw::log::LogFatal("lola")
                << "TracePointKey data strings (service_type_name and service_element_name) are too long: size"
                << input_value_size << "should be less than"
                << max_buffer_size - (service_element_type_size + sizeof(value.trace_point_type)) << ". Terminating.";
            std::terminate();
        }

        std::array<char, max_buffer_size> local_buffer{};
        const auto it_local_buffer_last_copy1 = std::copy(value.service_element.service_type_name.begin(),
                                                          value.service_element.service_type_name.end(),
                                                          local_buffer.begin());
        const auto it_local_buffer_last_copy2 = std::copy(value.service_element.service_element_name.begin(),
                                                          value.service_element.service_element_name.end(),
                                                          it_local_buffer_last_copy1);

        static_assert(sizeof(value.service_element.service_element_type) == sizeof(char),
                      "trace_point_type is not of size char");
        *it_local_buffer_last_copy2 = static_cast<char>(value.service_element.service_element_type);

        static_assert(sizeof(value.trace_point_type) == sizeof(char), "trace_point_type is not of size char");
        *std::next(it_local_buffer_last_copy2) = static_cast<char>(value.trace_point_type);

        std::string_view local_string_buffer{local_buffer.data(), input_value_size};
        return std::hash<std::string_view>{}(local_string_buffer);
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACE_POINT_KEY_H
