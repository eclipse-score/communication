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

#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"

#include "score/mw/com/gateway/transport_layer/sample/messages/serialization.h"

namespace score::mw::com::gateway
{

namespace
{

template <typename T>
std::size_t SerializeWithTemplate(const T& message, score::cpp::span<std::uint8_t> buffer)
{
    const auto size = gateway::ComputeSerializedSize(message);
    if (size > static_cast<std::size_t>(buffer.size()))
    {
        score::mw::log::LogError() << "Serialize buffer too small: need " << size << " have " << buffer.size();
        return 0U;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) uint8_t and std::byte have same representation
    score::cpp::span<std::byte> span(reinterpret_cast<std::byte*>(buffer.data()), size);
    auto result = gateway::Serialize(message, span);
    if (!result.has_value())
    {
        score::mw::log::LogError() << "Failed to serialize message of type " << static_cast<int>(message.GetType())
                                   << ": " << (result.error());
        return 0U;
    }
    return size;
}

template <typename T>
bool DeserializeWithTemplate(T& message, score::cpp::span<const std::uint8_t> data)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) uint8_t and std::byte have same representation
    score::cpp::span<const std::byte> span(reinterpret_cast<const std::byte*>(data.data()), data.size());
    auto result = gateway::Deserialize(message, span);
    return result.has_value();
}

}  // namespace

std::size_t ProvideServiceRequest::Serialize(score::cpp::span<std::uint8_t> buffer) const
{
    return SerializeWithTemplate(*this, buffer);
}

bool ProvideServiceRequest::Deserialize(score::cpp::span<const std::uint8_t> data)
{
    return DeserializeWithTemplate(*this, data);
}

std::size_t AckResponse::Serialize(score::cpp::span<std::uint8_t> buffer) const
{
    return SerializeWithTemplate(*this, buffer);
}

bool AckResponse::Deserialize(score::cpp::span<const std::uint8_t> data)
{
    return DeserializeWithTemplate(*this, data);
}

std::size_t OfferServiceRequest::Serialize(score::cpp::span<std::uint8_t> buffer) const
{
    return SerializeWithTemplate(*this, buffer);
}

bool OfferServiceRequest::Deserialize(score::cpp::span<const std::uint8_t> data)
{
    return DeserializeWithTemplate(*this, data);
}

std::size_t StopOfferServiceRequest::Serialize(score::cpp::span<std::uint8_t> buffer) const
{
    return SerializeWithTemplate(*this, buffer);
}

bool StopOfferServiceRequest::Deserialize(score::cpp::span<const std::uint8_t> data)
{
    return DeserializeWithTemplate(*this, data);
}

std::size_t ServiceElementMessage::Serialize(score::cpp::span<std::uint8_t> buffer) const
{
    return SerializeWithTemplate(*this, buffer);
}

bool ServiceElementMessage::Deserialize(score::cpp::span<const std::uint8_t> data)
{
    return DeserializeWithTemplate(*this, data);
}

}  // namespace score::mw::com::gateway
