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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_UNIQUE_METHOD_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_UNIQUE_METHOD_IDENTIFIER_H

#include "score/mw/com/impl/configuration/lola_field_id.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_method_or_field_id.h"
#include "score/mw/com/impl/method_type.h"
#include "score/mw/log/logging.h"

#include <cstdint>
#include <functional>
#include <type_traits>

namespace score::mw::com::impl::lola
{

struct UniqueMethodIdentifier
{
    static_assert(std::is_same_v<LolaMethodId, LolaFieldId>,
                  "LolaMethodId and LolaFieldId must be the same type for LolaMethodOrFieldId to work.");

    ::score::mw::com::impl::LolaMethodOrFieldId method_or_field_id{};
    ::score::mw::com::impl::MethodType method_type{::score::mw::com::impl::MethodType::kUnknown};
};

bool operator==(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept;
bool operator!=(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept;

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const UniqueMethodIdentifier& value) noexcept;
}  // namespace score::mw::com::impl::lola

namespace std
{
template <>
struct hash<score::mw::com::impl::lola::UniqueMethodIdentifier>
{
    std::size_t operator()(
        const score::mw::com::impl::lola::UniqueMethodIdentifier& unique_method_identifier) const noexcept
    {
        static constexpr std::uint32_t method_or_field_id_shift = 16U;
        return std::hash<std::uint32_t>{}(
            (static_cast<std::uint32_t>(unique_method_identifier.method_or_field_id) << method_or_field_id_shift) |
            static_cast<std::uint32_t>(unique_method_identifier.method_type));
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_UNIQUE_METHOD_IDENTIFIER_H
