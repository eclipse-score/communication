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
#include "score/mw/com/impl/configuration/coherency_type.h"

namespace score::mw::com::impl
{

std::ostream& operator<<(std::ostream& ostream_out, const CoherencyType& coherency_type)
{
    switch (coherency_type)
    {
        case CoherencyType::kNonCoherent:
            ostream_out << "NON_COHERENT";
            break;
        case CoherencyType::kCoherent:
            ostream_out << "COHERENT";
            break;
        case CoherencyType::kMixed:
            ostream_out << "MIXED";
            break;
        default:
            ostream_out << "(unknown)";
            break;
    }

    return ostream_out;
}

}  // namespace score::mw::com::impl
