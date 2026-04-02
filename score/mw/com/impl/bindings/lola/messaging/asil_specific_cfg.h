/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_ASIL_SPECIFIC_CFG_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_ASIL_SPECIFIC_CFG_H

#include <unistd.h>

#include <cstdint>
#include <vector>

namespace score::mw::com::impl::lola
{

/// \brief Aggregation of ASIL level specific/dependent config properties.
struct AsilSpecificCfg
{
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::int32_t message_queue_rx_size_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::vector<uid_t> allowed_user_ids_;
};
}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_ASIL_SPECIFIC_CFG_H
