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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_INSTANCE_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_INSTANCE_IDENTIFIER_H

#include "score/mw/com/impl/configuration/global_configuration.h"

#include "score/mw/log/logging.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace score::mw::com::impl::lola
{

/// \brief Struct containing the information that is required by a Skeleton instance to uniquely identify a Proxy
/// instance that is connected to it. I.e. with the assumption that the service ID and instance ID is already known by
/// the Skeleton.
///
/// There may be multiple Proxy instances with the same service ID and instance ID in the same process or in different
/// processes. Therefore, in order to uniquely identifer a Proxy instance, we use Application ID (which uniquely
/// identifies a process) and a Proxy instance counter, which is guaranteed to be unique per Proxy instance in a given
/// process.
struct ProxyInstanceIdentifier
{
    using ProxyInstanceCounter = std::uint16_t;

    // According to our configuration schema an application id has to be unique during runtime. Thus, there are no two
    // processes running with the same application id -> application id uniquely identifies a process.
    GlobalConfiguration::ApplicationId process_identifier;
    ProxyInstanceCounter proxy_instance_counter;
};

bool operator==(const ProxyInstanceIdentifier& lhs, const ProxyInstanceIdentifier& rhs) noexcept;

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const ProxyInstanceIdentifier& value) noexcept;

}  // namespace score::mw::com::impl::lola

namespace std
{

template <>
class hash<score::mw::com::impl::lola::ProxyInstanceIdentifier>
{
  public:
    std::size_t operator()(
        const score::mw::com::impl::lola::ProxyInstanceIdentifier& proxy_instance_identifier) const noexcept
    {
        using ProxyInstanceIdentifier = score::mw::com::impl::lola::ProxyInstanceIdentifier;
        static_assert(sizeof(ProxyInstanceIdentifier::process_identifier) +
                          sizeof(ProxyInstanceIdentifier::proxy_instance_counter) <
                      sizeof(std::uint64_t));

        constexpr auto proxy_instance_counter_bit_width =
            std::numeric_limits<decltype(proxy_instance_identifier.proxy_instance_counter)>::digits;
        return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(proxy_instance_identifier.process_identifier)
                                           << static_cast<std::uint64_t>(proxy_instance_counter_bit_width)) |
                                          static_cast<std::uint64_t>(proxy_instance_identifier.proxy_instance_counter));
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_INSTANCE_IDENTIFIER_H
