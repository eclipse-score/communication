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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_GATEWAY_MESSAGES_H_
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_GATEWAY_MESSAGES_H_

#include "score/mw/com/gateway/transport_layer/sample/messages/transport_message.h"
#include "score/mw/com/gateway/transport_layer/transport.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace score::mw::com::gateway
{

/// \brief Message to propagate service instance metadata (instance specifier + service element configurations)
/// from the source gateway to the destination gateway. The destination gateway uses this information to create a
/// (generic) skeleton for the given service instance.
class ProvideServiceRequest : public TransportMessage
{
    template <typename Self>
    static auto GetSerializeMembersImpl(Self& self)
    {
        using SelfNoRef = std::remove_reference_t<Self>;
        using StringType = std::conditional_t<std::is_const_v<SelfNoRef>, const std::string, std::string>;
        using VectorType = std::
            conditional_t<std::is_const_v<SelfNoRef>, const std::vector<impl::EventInfo>, std::vector<impl::EventInfo>>;
        using Uint32Type = std::conditional_t<std::is_const_v<SelfNoRef>, const std::uint32_t, std::uint32_t>;
        return std::tuple<StringType&, VectorType&, Uint32Type&, Uint32Type&>(
            self.instance_specifier_, self.elements_, self.shm_control_size_, self.shm_data_size_);
    }

  public:
    ProvideServiceRequest() : TransportMessage(MessageType::kProvideServiceRequest) {}

    ProvideServiceRequest(impl::InstanceSpecifier service_instance_specifier,
                          std::vector<impl::EventInfo> service_elements,
                          std::uint32_t shm_control_size = 0U,
                          std::uint32_t shm_data_size = 0U)
        : TransportMessage(MessageType::kProvideServiceRequest),
          instance_specifier_(std::string{service_instance_specifier.ToString()}),
          elements_(std::move(service_elements)),
          shm_control_size_(shm_control_size),
          shm_data_size_(shm_data_size)
    {
    }

    std::size_t Serialize(score::cpp::span<std::uint8_t> buffer) const override;
    bool Deserialize(score::cpp::span<const std::uint8_t> data) override;

    const std::string& GetInstanceSpecifier() const
    {
        return instance_specifier_;
    }

    const std::vector<impl::EventInfo>& GetServiceElements() const
    {
        return elements_;
    }

    std::uint32_t GetShmControlSize() const
    {
        return shm_control_size_;
    }

    std::uint32_t GetShmDataSize() const
    {
        return shm_data_size_;
    }

    auto GetSerializeMembers() const
    {
        return GetSerializeMembersImpl(*this);
    }
    auto GetSerializeMembers()
    {
        return GetSerializeMembersImpl(*this);
    }

  private:
    std::string instance_specifier_;
    std::vector<impl::EventInfo> elements_;
    std::uint32_t shm_control_size_{0U};
    std::uint32_t shm_data_size_{0U};
};

/// \brief Acknowledgement response message, sent to confirm receipt of a request with a given sequence number.
class AckResponse : public TransportMessage
{
  public:
    AckResponse() : TransportMessage(MessageType::kAckResponse), acked_sequence_(0) {}

    explicit AckResponse(const std::uint32_t acked_sequence)
        : TransportMessage(MessageType::kAckResponse), acked_sequence_(acked_sequence)
    {
    }

    std::size_t Serialize(score::cpp::span<std::uint8_t> buffer) const override;
    bool Deserialize(score::cpp::span<const std::uint8_t> data) override;

    std::uint32_t GetAckedSequence() const
    {
        return acked_sequence_;
    }

    void SetAckedSequence(std::uint32_t sequence)
    {
        acked_sequence_ = sequence;
    }

    std::tuple<const std::uint32_t&> GetSerializeMembers() const
    {
        return {acked_sequence_};
    }
    std::tuple<std::uint32_t&> GetSerializeMembers()
    {
        return {acked_sequence_};
    }

  private:
    std::uint32_t acked_sequence_;
};

/// \brief Message to trigger service-instance offering at the destination gateway side.
/// The service instance is expected to be already created at the destination gateway side,
/// e.g. by a previous ProvideServiceRequest.
class OfferServiceRequest : public TransportMessage
{
  public:
    OfferServiceRequest() : TransportMessage(MessageType::kOfferServiceRequest) {}

    explicit OfferServiceRequest(impl::InstanceSpecifier service_instance_specifier)
        : TransportMessage(MessageType::kOfferServiceRequest),
          instance_specifier_(std::string{service_instance_specifier.ToString()})
    {
    }

    std::size_t Serialize(score::cpp::span<std::uint8_t> buffer) const override;
    bool Deserialize(score::cpp::span<const std::uint8_t> data) override;

    const std::string& GetInstanceSpecifier() const
    {
        return instance_specifier_;
    }

    std::tuple<const std::string&> GetSerializeMembers() const
    {
        return {instance_specifier_};
    }
    std::tuple<std::string&> GetSerializeMembers()
    {
        return {instance_specifier_};
    }

  private:
    std::string instance_specifier_;
};

/// \brief Message to trigger service-instance stop-offer at the destination gateway side.
class StopOfferServiceRequest : public TransportMessage
{
  public:
    StopOfferServiceRequest() : TransportMessage(MessageType::kStopOfferServiceRequest) {}

    explicit StopOfferServiceRequest(impl::InstanceSpecifier service_instance_specifier)
        : TransportMessage(MessageType::kStopOfferServiceRequest),
          instance_specifier_(std::string{service_instance_specifier.ToString()})
    {
    }

    std::size_t Serialize(score::cpp::span<std::uint8_t> buffer) const override;
    bool Deserialize(score::cpp::span<const std::uint8_t> data) override;

    const std::string& GetInstanceSpecifier() const
    {
        return instance_specifier_;
    }

    std::tuple<const std::string&> GetSerializeMembers() const
    {
        return {instance_specifier_};
    }
    std::tuple<std::string&> GetSerializeMembers()
    {
        return {instance_specifier_};
    }

  private:
    std::string instance_specifier_;
};

/// \brief Base class for messages that identify a service element by instance specifier, type and name.
/// \details Used as a common base for RegisterNotificationRequest, UnregisterNotificationRequest,
/// SubscribeRequest, UnsubscribeRequest, and UpdateNotification messages.
class ServiceElementMessage : public TransportMessage
{
    template <typename Self>
    static auto GetSerializeMembersImpl(Self& self)
    {
        using SelfNoRef = std::remove_reference_t<Self>;
        using StringType = std::conditional_t<std::is_const_v<SelfNoRef>, const std::string, std::string>;
        using TypeType =
            std::conditional_t<std::is_const_v<SelfNoRef>, const impl::ServiceElementType, impl::ServiceElementType>;
        return std::tuple<StringType&, TypeType&, StringType&>(
            self.instance_specifier_, self.element_type_, self.element_name_);
    }

  public:
    using TransportMessage::TransportMessage;

    ServiceElementMessage(MessageType message_type,
                          impl::InstanceSpecifier service_instance_specifier,
                          impl::ServiceElementType element_type,
                          std::string element_name)
        : TransportMessage(message_type),
          instance_specifier_(std::string{service_instance_specifier.ToString()}),
          element_type_(element_type),
          element_name_(std::move(element_name))
    {
    }

    std::size_t Serialize(score::cpp::span<std::uint8_t> buffer) const override;
    bool Deserialize(score::cpp::span<const std::uint8_t> data) override;

    const std::string& GetInstanceSpecifier() const
    {
        return instance_specifier_;
    }
    impl::ServiceElementType GetElementType() const
    {
        return element_type_;
    }
    const std::string& GetElementName() const
    {
        return element_name_;
    }

    auto GetSerializeMembers() const
    {
        return GetSerializeMembersImpl(*this);
    }
    auto GetSerializeMembers()
    {
        return GetSerializeMembersImpl(*this);
    }

  private:
    std::string instance_specifier_;
    impl::ServiceElementType element_type_{impl::ServiceElementType::INVALID};
    std::string element_name_;
};

/// \brief Message to register for update notifications of a service element at the source gateway side.
/// \details The source gateway is expected to subscribe to the service element and forward update notifications
/// via UpdateNotification messages.
class RegisterNotificationRequest final : public ServiceElementMessage
{
  public:
    RegisterNotificationRequest() : ServiceElementMessage(MessageType::kRegisterNotificationRequest) {}
    RegisterNotificationRequest(impl::InstanceSpecifier service_instance_specifier,
                                impl::ServiceElementType element_type,
                                std::string element_name)
        : ServiceElementMessage(MessageType::kRegisterNotificationRequest,
                                std::move(service_instance_specifier),
                                element_type,
                                std::move(element_name))
    {
    }
};

/// \brief Message to unregister from update notifications of a service element at the source gateway side.
class UnregisterNotificationRequest final : public ServiceElementMessage
{
  public:
    UnregisterNotificationRequest() : ServiceElementMessage(MessageType::kUnregisterNotificationRequest) {}
    UnregisterNotificationRequest(impl::InstanceSpecifier service_instance_specifier,
                                  impl::ServiceElementType element_type,
                                  std::string element_name)
        : ServiceElementMessage(MessageType::kUnregisterNotificationRequest,
                                std::move(service_instance_specifier),
                                element_type,
                                std::move(element_name))
    {
    }
};

/// \brief Message used to notify about updates of service elements (e.g. new event samples, field value changes).
/// \details Sent from the source gateway to the destination gateway when a subscribed service element has new data.
class UpdateNotification final : public ServiceElementMessage
{
  public:
    UpdateNotification() : ServiceElementMessage(MessageType::kUpdateNotification) {}
    UpdateNotification(impl::InstanceSpecifier service_instance_specifier,
                       impl::ServiceElementType element_type,
                       std::string element_name)
        : ServiceElementMessage(MessageType::kUpdateNotification,
                                std::move(service_instance_specifier),
                                element_type,
                                std::move(element_name))
    {
    }
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_SAMPLE_GATEWAY_MESSAGES_H_
