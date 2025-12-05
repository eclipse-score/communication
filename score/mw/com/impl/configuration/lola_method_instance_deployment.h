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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_METHOD_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_METHOD_INSTANCE_DEPLOYMENT_H

#include "score/json/json_parser.h"

#include <cstdint>
#include <optional>

namespace score::mw::com::impl
{

/**
 * @brief Represents instance-specific deployment configuration for a LoLa method.
 *
 * This class encapsulates deployment parameters for a specific method instance within
 * a service instance.
 *
 * The class provides JSON serialization capabilities. Deserialization is handled through
 * the constructor and CreateFromJson factory method.
 */
class LolaMethodInstanceDeployment
{
  public:
    using QueueSize = std::uint8_t;

    /**
     * @brief Construct LolaMethodInstanceDeployment with optional queue size, because LolaMethodInstanceDeployment for
     * a consumer will have a value while one for a provider will not.
     * @param queue_size The maximum number of pending method requests that can be queued.
     */
    explicit LolaMethodInstanceDeployment(std::optional<QueueSize> queue_size);

    explicit LolaMethodInstanceDeployment(const score::json::Object& serialized_lola_method_instance_deployment);

    static LolaMethodInstanceDeployment CreateFromJson(
        const score::json::Object& serialized_lola_method_instance_deployment);

    /**
     * @brief Serializes the deployment configuration to a JSON object.
     * @return A JSON object representing the method instance deployment.
     */
    score::json::Object Serialize() const;

    /**
     * @brief Version number of the serialization format.
     *
     * This constant is used to track the version of the serialization format for
     * backward compatibility. If the format changes in future versions, this number
     * should be incremented.
     */
    constexpr static std::uint8_t serializationVersion{1U};

    /**
     * @brief The maximum number of method requests that can be queued on the server side.
     */
    std::optional<QueueSize> queue_size_{std::nullopt};
};

inline bool operator==(const LolaMethodInstanceDeployment& lhs, const LolaMethodInstanceDeployment& rhs) noexcept
{
    return lhs.queue_size_ == rhs.queue_size_;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_METHOD_INSTANCE_DEPLOYMENT_H
