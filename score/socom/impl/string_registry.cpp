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

#include "score/socom/string_registry.hpp"

#include <algorithm>

namespace score::socom {

// This insert should be used for inserting compile time string literals into the registry.
std::pair<Registry_string_view, bool> String_registry::insert(
    std::string_view const new_string, Literal_tag /*is_static_string_literal*/) noexcept {
    std::lock_guard<std::mutex> const locked{m_mutex};

    // emplace will already search for previously existing entry
    auto const registered_string = m_registered_strings.emplace(new_string);
    return std::pair<Registry_string_view, bool>{Registry_string_view{*registered_string.first},
                                                 registered_string.second};
}

std::pair<Registry_string_view, bool>
// NOLINTNEXTLINE(bugprone-exception-escape): All exceptions are either handled or left unhandled as
// a design decision.
String_registry::insert(std::string_view const new_string) noexcept {
    std::lock_guard<std::mutex> const locked{m_mutex};

    auto const iter = m_registered_strings.find(new_string);
    if (iter == m_registered_strings.end()) {
        auto& inserted_string = m_dynamic_allocated.emplace_front(new_string);
        auto const registered_string = m_registered_strings.insert(inserted_string);
        return std::pair<Registry_string_view, bool>{Registry_string_view{*registered_string.first},
                                                     registered_string.second};
    }

    return std::pair<Registry_string_view, bool>{Registry_string_view{*iter}, false};
}

std::pair<Registry_string_view, bool> String_registry::insert(std::string&& new_string) noexcept {
    std::lock_guard<std::mutex> const locked{m_mutex};

    auto const iter =
        std::find_if(m_registered_strings.begin(), m_registered_strings.end(),
                     [&new_string](auto const& entry) { return entry.data() == new_string; });

    if (iter == m_registered_strings.end()) {
        auto& inserted_string = m_dynamic_allocated.emplace_front(std::move(new_string));
        auto const registered_string = m_registered_strings.insert(inserted_string);
        return std::pair<Registry_string_view, bool>{Registry_string_view{*registered_string.first},
                                                     registered_string.second};
    }

    return std::pair<Registry_string_view, bool>{Registry_string_view{*iter}, false};
}

String_registry& service_id_registry() noexcept {
    static String_registry registry;
    return registry;
}

String_registry& instance_id_registry() noexcept {
    static String_registry registry;
    return registry;
}

}  // namespace score::socom
