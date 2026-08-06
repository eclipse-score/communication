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

#include "service_identifier.hpp"

#include <tuple>

namespace score::socom
{

bool operator<(const Service_instance_identifier& lhs, const Service_instance_identifier& rhs)
{
    return std::tie(lhs.instance, lhs.interface) < std::tie(rhs.instance, rhs.interface);
}

}  // namespace score::socom
