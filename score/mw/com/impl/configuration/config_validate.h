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

#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_VALIDATE_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_VALIDATE_H

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::impl::configuration
{

// A common function for emplacing an element into a map and terminating the program if the element already exists in
// the map. shall be used by both strategies (JSON & Flatbuffers) parsers.
template <typename Map, typename Key, typename Value>
void EmplaceOrFatal(Map& map, Key&& key, Value&& value, std::string_view element_description)
{
    const auto result = map.emplace(std::forward<Key>(key), std::forward<Value>(value));
    if (!result.second)
    {
        score::mw::log::LogFatal("lola") << element_description << " was configured twice.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

void ValidateUniqueServiceElementIds(const LolaServiceTypeDeployment& deployment);
InstanceSpecifier CreateValidInstanceSpecifier(std::string instance_specifier_name);

template <typename Container>
void ValidateSingleDeployment(const Container& deployments, const ServiceIdentifierType& service_identifier)
{
    if (deployments.size() != 1U)
    {
        score::mw::log::LogFatal("lola") << "More or less then one deployment for " << service_identifier.ToString()
                                         << ". Multi-Binding support right now not supported";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
}

void CrosscheckAsilLevels(const Configuration& config);
void CrosscheckServiceInstancesToTypes(const Configuration& config);

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIG_VALIDATE_H
