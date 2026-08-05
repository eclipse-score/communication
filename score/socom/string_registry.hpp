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

#ifndef SRC_SOCOM_INCLUDE_SCORE_SOCOM_STRING_REGISTRY
#define SRC_SOCOM_INCLUDE_SCORE_SOCOM_STRING_REGISTRY

#include <forward_list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "score/socom/registry_string_view.hpp"

namespace score::socom {

/// \brief Tag to select StringView literal version
struct Literal_tag {};

///
/// \class String_registry
///
/// \brief A central registry for strings to avoid copying and to facilitate cheap comparison.
///
class String_registry final {
   public:
    ///
    /// \brief Insert a new StringView literal into the string registry.
    ///
    /// \param[in]      new_string        String to be added to the registry.
    /// \param[in]      (no name)         Used only to select StringView literal version of the
    /// method.
    ///
    /// \return String_view of the string in registry and boolean denoting whether the string was
    ///         newly added (true) or was present already (false).
    ///
    std::pair<Registry_string_view, bool> insert(std::string_view new_string,
                                                 Literal_tag /*is_static_string_literal*/) noexcept;

    ///
    /// \brief Insert a new StringView into the string registry.
    ///
    /// \param[in]      new_string        String to be added to the registry.
    ///
    /// \return String_view of the string in registry and boolean denoting whether the string was
    ///         newly added (true) or was present already (false).
    ///
    // NOLINTNEXTLINE(bugprone-exception-escape): If exception is thrown it shall be considered as
    // fatal error and std::terminate is desired behavior.
    std::pair<Registry_string_view, bool> insert(std::string_view new_string) noexcept;

    ///
    /// \brief Insert a new new std::string into the string registry.
    ///
    /// \param[in]      new_string        String to be added to the registry.
    ///
    /// \return String_view of the string in registry and boolean denoting whether the string was
    ///         newly added (true) or was present already (false).
    ///
    std::pair<Registry_string_view, bool> insert(std::string&& new_string) noexcept;

   private:
    std::unordered_set<std::string_view> m_registered_strings;
    std::forward_list<std::string> m_dynamic_allocated;
    std::mutex m_mutex;
};

String_registry& service_id_registry() noexcept;

String_registry& instance_id_registry() noexcept;

}  // namespace score::socom

#endif  // SRC_SOCOM_INCLUDE_SCORE_SOCOM_STRING_REGISTRY
