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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SERVICE_ELEMENT_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SERVICE_ELEMENT_IDENTIFIER_H

#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/impl/tracing/configuration/hash_helper_for_service_element_and_se_view.h"

#include "score/mw/log/logging.h"

#include <array>
#include <cstring>
#include <exception>
#include <string>

namespace score::mw::com::impl::tracing
{

/// \brief Binding independent unique identifier of a service element (I.e. event, field, method) which contains owning
/// strings.
///
/// A ServiceElementIdentifier cannot differentiate between the same service elements of different instances. For
/// that, ServiceElementInstanceIdentifierView should be used.
// Suppress "AUTOSAR C++14 A11-0-2" rule finding.
// This rule states: "A type defined as struct shall: (1) provide only public data members, (2)
// not provide any special member functions or methods, (3) not be a base of
// another struct or class, (4) not inherit from another struct or class.".
// Rationale: The Coverity tool complains because this struct is not compliant to POD type containing non-POD member.
// Therefore, the class should be instead. However, public access is required by the implementation for simplicity.
// Despite this, none of the rule's requirements are violated.
// coverity[autosar_cpp14_a11_0_2_violation]
struct ServiceElementIdentifier
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::string service_type_name;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::string service_element_name;
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceElementType service_element_type;
};

bool operator==(const ServiceElementIdentifier& lhs, const ServiceElementIdentifier& rhs) noexcept;
bool operator<(const ServiceElementIdentifier& lhs, const ServiceElementIdentifier& rhs) noexcept;

::score::mw::log::LogStream& operator<<(::score::mw::log::LogStream& log_stream,
                                      const ServiceElementIdentifier& service_element_identifier);

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
struct hash<score::mw::com::impl::tracing::ServiceElementIdentifier>
// NOLINTEND(score-struct-usage-compliance): STL defines hash as struct.
{
    size_t operator()(const score::mw::com::impl::tracing::ServiceElementIdentifier& value) const noexcept
    {
        return score::mw::com::impl::tracing::hash_helper(value);
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_SERVICE_ELEMENT_IDENTIFIER_H
