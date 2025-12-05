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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_SKELETON_INSTANCE_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_SKELETON_INSTANCE_IDENTIFIER_H

#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace score::mw::com::impl::lola
{

/// \brief Struct containing the information that is required to uniquely identifer a Skeleton instance.
///
/// There can only be one Skeleton instance with a given service ID and instance ID across all processes. Therefore,
/// these are sufficient to uniquely identifer a Skeleton instance.
struct SkeletonInstanceIdentifier
{
    LolaServiceId service_id;
    LolaServiceInstanceId::InstanceId instance_id;
};

bool operator==(const SkeletonInstanceIdentifier& lhs, const SkeletonInstanceIdentifier& rhs) noexcept;

}  // namespace score::mw::com::impl::lola

namespace std
{

template <>
class hash<score::mw::com::impl::lola::SkeletonInstanceIdentifier>
{
  public:
    std::size_t operator()(
        const score::mw::com::impl::lola::SkeletonInstanceIdentifier& skeleton_instance_identifier) const noexcept
    {
        using SkeletonInstanceIdentifier = score::mw::com::impl::lola::SkeletonInstanceIdentifier;
        static_assert(sizeof(SkeletonInstanceIdentifier::service_id) + sizeof(SkeletonInstanceIdentifier::instance_id) <
                      sizeof(std::uint64_t));

        constexpr auto instance_id_bit_width =
            std::numeric_limits<decltype(skeleton_instance_identifier.instance_id)>::digits;
        return std::hash<std::uint64_t>{}(
            (static_cast<std::uint64_t>(skeleton_instance_identifier.service_id) << instance_id_bit_width) |
            static_cast<std::uint64_t>(skeleton_instance_identifier.instance_id));
    }
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_SKELETON_INSTANCE_IDENTIFIER_H
