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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_METHOD_INSTANCE_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_METHOD_INSTANCE_IDENTIFIER_H

#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"

#include "score/mw/log/logging.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace score::mw::com::impl::lola
{

/// \brief Struct containing the information that is required to uniquely identifer a ProxyMethod instance.
struct ProxyMethodInstanceIdentifier
{
    ProxyInstanceIdentifier proxy_instance_identifier;
    LolaMethodId method_id;
};

bool operator==(const ProxyMethodInstanceIdentifier& lhs, const ProxyMethodInstanceIdentifier& rhs) noexcept;

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const ProxyMethodInstanceIdentifier& value) noexcept;

}  // namespace score::mw::com::impl::lola

namespace std
{

template <>
class hash<score::mw::com::impl::lola::ProxyMethodInstanceIdentifier>
{
  public:
    std::size_t operator()(
        const score::mw::com::impl::lola::ProxyMethodInstanceIdentifier& proxy_method_instance_identifier) const noexcept
    {
        using ProxyMethodInstanceIdentifier = score::mw::com::impl::lola::ProxyMethodInstanceIdentifier;
        using ProxyInstanceIdentifier = score::mw::com::impl::lola::ProxyInstanceIdentifier;
        static_assert((sizeof(ProxyInstanceIdentifier::process_identifier) +
                       sizeof(ProxyInstanceIdentifier::proxy_instance_counter) +
                       sizeof(ProxyMethodInstanceIdentifier::method_id)) <= sizeof(std::uint64_t));

        constexpr auto proxy_instance_counter_bit_width = std::numeric_limits<
            decltype(proxy_method_instance_identifier.proxy_instance_identifier.proxy_instance_counter)>::digits;
        constexpr auto method_id_bit_width =
            std::numeric_limits<decltype(proxy_method_instance_identifier.method_id)>::digits;
        return std::hash<std::uint64_t>{}(
            (static_cast<std::uint64_t>(proxy_method_instance_identifier.proxy_instance_identifier.process_identifier)
             << (proxy_instance_counter_bit_width + method_id_bit_width)) |
            (static_cast<std::uint64_t>(
                 proxy_method_instance_identifier.proxy_instance_identifier.proxy_instance_counter)
             << method_id_bit_width) |
            static_cast<std::uint64_t>(proxy_method_instance_identifier.method_id));
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_METHOD_INSTANCE_IDENTIFIER_H
