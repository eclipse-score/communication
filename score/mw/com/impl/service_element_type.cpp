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
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl
{

// Suppress "AUTOSAR C++14 A13-2-2" rule finding: "A binary arithmetic operator and a bitwise operator shall return
// a “prvalue”."
// The code here is present in single line to avoid '<<' is not a left shift operator but an overload for logging the
// respective types. code analysis tools tend to assume otherwise hence a false positive.
// coverity[autosar_cpp14_a13_2_2_violation]
::score::mw::log::LogStream& operator<<(::score::mw::log::LogStream& log_stream,
                                      const ServiceElementType& service_element_type)
{
    switch (service_element_type)
    {
        case ServiceElementType::INVALID:
            log_stream << "INVALID";
            break;
        case ServiceElementType::EVENT:
            log_stream << "EVENT";
            break;
        case ServiceElementType::FIELD:
            log_stream << "FIELD";
            break;
        default:
            log_stream << "UNKNOWN";
            break;
    }
    return log_stream;
}

}  // namespace score::mw::com::impl
