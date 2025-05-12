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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_ID_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_ID_H

#include "score/json/json_parser.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Struct that wraps the type of a LoLa instance ID.
///
/// Since LolaServiceInstanceId is held in a variant within ServiceInstanceId, we use a struct so that we can
/// unambiguously differentiate between the different InstanceId types when visiting the struct.
class LolaServiceInstanceId
{
  public:
    using InstanceId = std::uint16_t;

    explicit LolaServiceInstanceId(const score::json::Object& json_object) noexcept;
    explicit LolaServiceInstanceId(InstanceId instance_id) noexcept;

    score::json::Object Serialize() const noexcept;
    std::string_view ToHashString() const noexcept;

    /**
     * \brief The size of the hash string returned by ToHashString()
     *
     * The size is the amount of chars required to represent InstanceId as a hex string.
     */
    constexpr static std::size_t hashStringSize{2U * sizeof(InstanceId)};

    constexpr static std::uint32_t serializationVersion{1U};

    InstanceId GetId() const
    {
        return id_;
    }

  private:
    InstanceId id_;
    /**
     * \brief Stringified format of this LolaServiceInstanceId which can be used for hashing.
     */
    std::string hash_string_;
};

bool operator==(const LolaServiceInstanceId& lhs, const LolaServiceInstanceId& rhs) noexcept;
bool operator<(const LolaServiceInstanceId& lhs, const LolaServiceInstanceId& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_SERVICE_INSTANCE_ID_H
