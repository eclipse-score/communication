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

#include "score/mw/com/impl/bindings/lola/methods/unique_method_identifier.h"
#include "score/mw/com/impl/bindings/lola/proxy_instance_identifier.h"

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
    UniqueMethodIdentifier unique_method_identifier;
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
    std::size_t operator()(const score::mw::com::impl::lola::ProxyMethodInstanceIdentifier&
                               proxy_method_instance_identifier) const noexcept
    {
        using UniqueMethodIdentifier = score::mw::com::impl::lola::UniqueMethodIdentifier;
        using ProxyInstanceIdentifier = score::mw::com::impl::lola::ProxyInstanceIdentifier;

        static constexpr std::size_t kGoldenRatioConstant = 0x9e3779b9U;
        static constexpr unsigned int kLeftShiftBits = 6U;
        static constexpr unsigned int kRightShiftBits = 2U;

        // The previous implementation packed all fields into a single uint64_t, but with the addition of
        // MethodType to UniqueMethodIdentifier, the total bit width (72 bits) exceeds 64 bits.
        // We now combine the hashes of the sub-structs instead using the boost hash_combine pattern.
        auto seed = std::hash<ProxyInstanceIdentifier>{}(proxy_method_instance_identifier.proxy_instance_identifier);
        seed ^= std::hash<UniqueMethodIdentifier>{}(proxy_method_instance_identifier.unique_method_identifier) +
                kGoldenRatioConstant + (seed << kLeftShiftBits) + (seed >> kRightShiftBits);
        return seed;
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_PROXY_METHOD_INSTANCE_IDENTIFIER_H
