/*********************************************************************************
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
 *******************************************************************************/

#include "score/mw/service/provided_services_container.h"

namespace score::mw::service
{

ProvidedServicesContainer::~ProvidedServicesContainer() noexcept
{
    StopAll();
}

void ProvidedServicesContainer::Reserve(const std::size_t size)
{
    services_.reserve(size);
}

void ProvidedServicesContainer::StopAll() noexcept
{
    for (auto& [_, service] : services_)
    {
        if (service != nullptr)
        {
            service->Stop();
        }
    }
    services_.clear();
}

}  // namespace score::mw::service
