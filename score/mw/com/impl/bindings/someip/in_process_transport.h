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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_IN_PROCESS_TRANSPORT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_IN_PROCESS_TRANSPORT_H

#include "score/mw/com/impl/bindings/someip/element_fq_id.h"
#include "score/mw/com/impl/bindings/someip/i_transport.h"

#include <score/callback.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl::someip
{

/// \brief Process-local stub implementation of ITransport.
///
/// \attention This is a STUB. It implements no part of the SOME/IP wire protocol: offered service elements are only
///            visible within the same process and sent payloads are copied into a process-local buffer instead of
///            being serialized and transmitted. It exists so that the SOME/IP binding can be built and unit-tested
///            end-to-end on the provider side while the real stack is not yet integrated. Nothing outside this class
///            may rely on its in-process nature.
class InProcessTransport final : public ITransport
{
  public:
    /// \brief Returns the process wide instance used by SOME/IP skeletons which are not given an explicit transport.
    static InProcessTransport& instance() noexcept;

    InProcessTransport() = default;
    ~InProcessTransport() override;

    Result<void> OfferEvent(const ElementFqId& element_fq_id) override;
    void StopOfferEvent(const ElementFqId& element_fq_id) override;
    Result<void> SendEvent(const ElementFqId& element_fq_id, const void* data, std::size_t size) override;
    Result<void> NotifyEvent(const ElementFqId& element_fq_id) override;
    bool HasSubscribers(const ElementFqId& element_fq_id) const override;

    /// \brief Registers a subscriber for the given service element. Test/consumer facing API of the stub.
    /// \return A subscription id which has to be handed to Unsubscribe().
    Result<std::uint32_t> Subscribe(const ElementFqId& element_fq_id);

    /// \brief Removes a previously registered subscriber.
    void Unsubscribe(const ElementFqId& element_fq_id, std::uint32_t subscription_id);

    /// \brief Returns a copy of the payload of the last sample sent for the given service element.
    std::vector<std::byte> GetLastSentPayload(const ElementFqId& element_fq_id) const;

    /// \brief Returns how often NotifyEvent() was called for the given service element.
    std::size_t GetNotificationCount(const ElementFqId& element_fq_id) const;

    /// \brief Drops all offered service elements, subscriptions and recorded payloads.
    void Reset();

  private:
    class OfferedEvent
    {
      public:
        std::vector<std::byte> last_payload{};
        std::vector<std::uint32_t> subscription_ids{};
        std::size_t notification_count{0U};
    };

    mutable std::mutex mutex_{};
    std::unordered_map<ElementFqId, OfferedEvent> offered_events_{};
    std::uint32_t next_subscription_id_{0U};
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_IN_PROCESS_TRANSPORT_H
