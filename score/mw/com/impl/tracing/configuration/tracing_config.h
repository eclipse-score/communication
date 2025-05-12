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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_CONFIG_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_CONFIG_H

#include <string>

namespace score::mw::com::impl::tracing
{

struct TracingConfig
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool enabled;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::string application_instance_id;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::string trace_filter_config_path;
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_CONFIG_H
