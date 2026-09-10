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
#include "score/mw/com/impl/configuration/config_validate.h"

#include <string>
#include <utility>

namespace score::mw::com::impl::configuration
{

InstanceSpecifier CreateValidInstanceSpecifier(std::string instance_specifier_name)
{
    auto result = InstanceSpecifier::Create(std::move(instance_specifier_name));
    if (!result.has_value())
    {
        score::mw::log::LogFatal("lola") << "Invalid InstanceSpecifier.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return result.value();
}

}  // namespace score::mw::com::impl::configuration
