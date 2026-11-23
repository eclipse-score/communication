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
#include "score/someip/routing_module/lola_mapping/lola_mapping.h"

#include <string_view>

namespace score::someip::routing_module
{
namespace
{

std::string Hex4(const std::uint16_t value)
{
    constexpr std::string_view kDigits{"0123456789abcdef"};
    std::string text(4U, '0');
    for (std::size_t index = 0U; index < text.size(); ++index)
    {
        const std::uint32_t nibble = (static_cast<std::uint32_t>(value) >> (4U * index)) & 0xFU;
        text[text.size() - 1U - index] = kDigits[nibble];
    }
    return text;
}

}  // namespace

std::string LolaInstanceSpecifier(const std::uint16_t service, const std::uint16_t instance)
{
    return "someip/" + Hex4(service) + "/" + Hex4(instance);
}

std::string LolaEventName(const std::uint16_t event)
{
    return "event_" + Hex4(event);
}

}  // namespace score::someip::routing_module
