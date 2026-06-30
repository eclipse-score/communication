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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_LOLA_PROXY_ELEMENT_BUILDING_BLOCKS_H
#define SCORE_MW_COM_IMPL_PLUMBING_LOLA_PROXY_ELEMENT_BUILDING_BLOCKS_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/service_element_type.h"

#include <string_view>

namespace score::mw::com::impl
{

lola::ElementFqId GetElementFqId(const HandleType& handle,
                                 const LolaServiceTypeDeployment& lola_type_deployment,
                                 const std::string_view service_element_name,
                                 const ServiceElementType element_type);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_LOLA_PROXY_ELEMENT_BUILDING_BLOCKS_H
