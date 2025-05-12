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
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl::tracing
{

bool operator==(const ServiceElementInstanceIdentifierView& lhs,
                const ServiceElementInstanceIdentifierView& rhs) noexcept
{
    return ((lhs.service_element_identifier_view == rhs.service_element_identifier_view) &&
            (lhs.instance_specifier == rhs.instance_specifier));
}
// Suppress "AUTOSAR C++14 A13-2-2" rule finding: "A binary arithmetic operator and a bitwise operator shall return
// a “prvalue”."
// The code here is present in single line to avoid '<<' is not a left shift operator but an overload for logging the
// respective types. code analysis tools tend to assume otherwise hence a false positive.
// coverity[autosar_cpp14_a13_2_2_violation]
::score::mw::log::LogStream& operator<<(
    ::score::mw::log::LogStream& log_stream,
    const ServiceElementInstanceIdentifierView& service_element_instance_identifier_view)
{
    log_stream << "service id: " << service_element_instance_identifier_view.service_element_identifier_view
               << ", instance id: " << service_element_instance_identifier_view.instance_specifier;
    return log_stream;
}

}  // namespace score::mw::com::impl::tracing
