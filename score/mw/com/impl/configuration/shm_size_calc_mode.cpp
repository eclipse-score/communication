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
#include "score/mw/com/impl/configuration/shm_size_calc_mode.h"

namespace score::mw::com::impl
{

std::ostream& operator<<(std::ostream& ostream_out, const ShmSizeCalculationMode& mode)
{
    switch (mode)
    {
        case ShmSizeCalculationMode::kSimulation:
            ostream_out << "SIMULATION";
            break;
        default:
            ostream_out << "(unknown)";
            break;
    }

    return ostream_out;
}

}  // namespace score::mw::com::impl
