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
#ifndef SCORE_MW_COM_IMPL_BINDING_TYPE_H
#define SCORE_MW_COM_IMPL_BINDING_TYPE_H

#include <cstdint>

namespace score::mw::com::impl
{

enum class BindingType : std::uint8_t
{
    kLoLa = 0x00,
    kFake = 0x01,
    kSomeIp = 0x02,
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_BINDING_TYPE_H
