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
#include "score/mw/com/impl/configuration/configuration_common_resources.h"

namespace score::mw::com::impl
{

// There is a template-method in service_type_deployment.cpp with the same signature, but different due to template use
// coverity[autosar_cpp14_a2_10_4_violation] False positive, see above
std::string ToHashStringImpl(const std::uint16_t instance_id, const std::size_t hash_string_size) noexcept
{
    /// \todo When we can use C++17 features, use std::to_chars so that we can convert from an int to a hex string
    /// with no dynamic allocations.
    std::stringstream stream;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(hash_string_size <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    // Passing std::hex to std::stringstream object with the stream operator follows the idiomatic way that both
    // features in conjunction were designed in the C++ standard.
    // coverity[autosar_cpp14_m8_4_4_violation] See above
    stream << std::setfill('0') << std::setw(static_cast<int>(hash_string_size)) << std::hex << instance_id;
    return stream.str();
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// coverity reports that ToBuffer().value might throw due to std::bad_variant_access. Which will cause an implicit
// terminate call.
// The .value() call will only throw if the ToBuffer returns an error. In this case we are in an unrecoverable state and
// a terminate callfunction didn't succeed in parsing the json. In this case termination is the intended behaviour.
// coverity[autosar_cpp14_a15_5_3_violation]
std::string ToStringImpl(const json::Object& serialized_json_object) noexcept
{
    score::json::JsonWriter writer{};
    const std::string serialized_form = writer.ToBuffer(serialized_json_object).value();
    return serialized_form;
}

}  // namespace score::mw::com::impl
