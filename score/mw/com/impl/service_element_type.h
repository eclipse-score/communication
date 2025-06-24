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
#ifndef SCORE_MW_COM_IMPL_SERVICE_ELEMENT_TYPE_H
#define SCORE_MW_COM_IMPL_SERVICE_ELEMENT_TYPE_H

#include <cstdint>

namespace score::mw::com::impl
{

/// \brief enum used to differentiate between difference service element types
enum class ServiceElementType : std::uint8_t
{
    INVALID = 0,
    EVENT,
    FIELD
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERVICE_ELEMENT_TYPE_H
