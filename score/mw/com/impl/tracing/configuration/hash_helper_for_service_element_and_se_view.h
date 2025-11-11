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
#ifndef SCORE_MW_COM_IMPL_TRACING_STD_HASH_OVERLOAD_FOR_SERVICE_ELEMENT_AND_SE_VIEW_H
#define SCORE_MW_COM_IMPL_TRACING_STD_HASH_OVERLOAD_FOR_SERVICE_ELEMENT_AND_SE_VIEW_H

#include "score/mw/com/impl/tracing/configuration/hash_helper_utility.h"
#include "score/mw/log/logging.h"

#include <array>
#include <string_view>
#include <type_traits>

namespace score::mw::com::impl::tracing
{
struct ServiceElementIdentifierView;
struct ServiceElementIdentifier;

// score::mw::com::impl::tracing::ServiceElementIdentifierView
template <typename T,
          typename = std::enable_if<std::is_same_v<T, score::mw::com::impl::tracing::ServiceElementIdentifier> ||
                                    std::is_same_v<T, score::mw::com::impl::tracing::ServiceElementIdentifierView>>>
std::size_t hash_helper(const T& value) noexcept
{
    // To prevent dynamic memory allocations, we copy the input ServiceElementIdentifierView elements into a local
    // buffer and create a string view to the local buffer. We then calculate the hash of the string_view.
    constexpr std::size_t max_buffer_size{1024U};

    const std::size_t size_to_copy_1 = value.service_type_name.size();
    const std::size_t size_to_copy_2 = value.service_element_name.size();
    constexpr auto service_element_type_size =
        sizeof(static_cast<std::underlying_type_t<decltype(value.service_element_type)>>(value.service_element_type));

    const std::array<std::size_t, 3U> input_values{{size_to_copy_1, size_to_copy_2, service_element_type_size}};
    const auto [input_value_size, data_overflow_error] = score::mw::com::impl::tracing::configuration::Accumulate(
        input_values.begin(),
        input_values.end(),
        // std::size_t{0U} triggers false positive A5-2-2 violation, see Ticket-161711
        std::size_t{});
    if (data_overflow_error || (input_value_size > max_buffer_size))
    {
        score::mw::log::LogFatal() << "ServiceElementIdentifier data strings (service_type_name and "
                                      "service_element_name) are too long: size"
                                   << input_value_size << "should be less than"
                                   << (max_buffer_size - service_element_type_size) << ". Terminating.";
        std::terminate();
    }

    std::array<char, max_buffer_size> local_buffer{};

    const auto it_local_buffer_last_copy1 =
        std::copy(value.service_type_name.begin(), value.service_type_name.end(), local_buffer.begin());
    const auto it_local_buffer_last_copy2 =
        std::copy(value.service_element_name.begin(), value.service_element_name.end(), it_local_buffer_last_copy1);

    static_assert(service_element_type_size == sizeof(char), "service_element_type is not of size char");
    *it_local_buffer_last_copy2 = static_cast<char>(value.service_element_type);

    std::string_view local_string_buffer{local_buffer.data(), input_value_size};
    return std::hash<std::string_view>{}(local_string_buffer);
}

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_STD_HASH_OVERLOAD_FOR_SERVICE_ELEMENT_AND_SE_VIEW_H
