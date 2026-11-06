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
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_CONFIG_CONFIGPARSE_H
#define LG_SOMEIP_CONFIG_CONFIGPARSE_H

#include <cstddef>
#include <cstring>
#include <exception/Exception.h>
#include <limits>
#include <rapidjson/document.h>
#include <stdexcept>
#include <string>

namespace lgsomeip {

// Reads an unsigned configuration value given as a JSON number or a numeric string in the given base.
// Throws a configuration error for non-numeric, trailing-garbage and out-of-range values instead of
// silently truncating them to Type.
template <typename Type> Type parse_unsigned(const rapidjson::Value& value, const char* member, int base) {
    unsigned long parsed = 0;
    if (value.IsUint()) {
        parsed = value.GetUint();
    } else if (value.IsString()) {
        std::size_t processed = 0;
        try {
            parsed = std::stoul(value.GetString(), &processed, base);
        } catch (const std::exception&) {
            throw LSAR_CONFIGURATION_ERROR(std::string("invalid numeric value for '") + member + "'");
        }
        if (processed != std::strlen(value.GetString())) {
            throw LSAR_CONFIGURATION_ERROR(std::string("invalid numeric value for '") + member + "'");
        }
    } else {
        throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' must be numeric");
    }

    if (parsed > std::numeric_limits<Type>::max()) {
        throw LSAR_CONFIGURATION_ERROR(std::string("numeric value for '") + member + "' is out of range");
    }
    return static_cast<Type>(parsed);
}

} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_CONFIGPARSE_H
