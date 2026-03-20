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
#include "score/mw/com/impl/tracing/configuration/service_element_identifier_view.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl::tracing
{

bool operator==(const ServiceElementIdentifierView& lhs, const ServiceElementIdentifierView& rhs) noexcept
{
    return std::tie(lhs.service_type_name, lhs.service_element_name, lhs.service_element_type) ==
           std::tie(rhs.service_type_name, rhs.service_element_name, rhs.service_element_type);
}

bool operator<(const ServiceElementIdentifierView& lhs, const ServiceElementIdentifierView& rhs) noexcept
{
    if (lhs.service_type_name != rhs.service_type_name)
    {
        return lhs.service_type_name < rhs.service_type_name;
    }
    else if (lhs.service_element_name != rhs.service_element_name)
    {
        return lhs.service_element_name < rhs.service_element_name;
    }
    else
    {
        return lhs.service_element_type < rhs.service_element_type;
    }
}
// Suppress "AUTOSAR C++14 A13-2-2" rule finding: "A binary arithmetic operator and a bitwise operator shall return
// a “prvalue”."
// The code here is present in single line to avoid '<<' is not a left shift operator but an overload for logging the
// respective types. code analysis tools tend to assume otherwise hence a false positive.
// coverity[autosar_cpp14_a13_2_2_violation]
::score::mw::log::LogStream& operator<<(::score::mw::log::LogStream& log_stream,
                                        const ServiceElementIdentifierView& service_element_identifier_view)
{
    log_stream << "service type: " << service_element_identifier_view.service_element_type
               << ", service element: " << service_element_identifier_view.service_element_name
               << ", service element type: " << service_element_identifier_view.service_element_type;
    return log_stream;
}

}  // namespace score::mw::com::impl::tracing
