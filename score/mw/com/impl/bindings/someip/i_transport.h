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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_I_TRANSPORT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_I_TRANSPORT_H

#include "score/mw/com/impl/bindings/someip/element_fq_id.h"

#include "score/result/result.h"

#include <cstddef>

namespace score::mw::com::impl::someip
{

/// \brief Abstraction of the SOME/IP transport used by the SOME/IP binding.
///
/// \details This is the single seam between the binding and an actual SOME/IP stack. The binding above it must not
///          assume anything about how (or whether) the payload reaches a remote consumer; it only offers/stop-offers
///          service elements and pushes opaque byte payloads.
///
/// \attention The only implementation available today is InProcessTransport, which is a stub: it keeps the last
///            payload per service element in process-local memory and never touches the network. Replacing it with a
///            real SOME/IP stack must not require changes above this interface.
class ITransport
{
  public:
    ITransport() = default;
    virtual ~ITransport();

    ITransport(const ITransport&) = delete;
    ITransport(ITransport&&) noexcept = delete;
    ITransport& operator=(const ITransport&) & = delete;
    ITransport& operator=(ITransport&&) & noexcept = delete;

    /// \brief Announces the given service element, so that consumers can subscribe to it.
    virtual Result<void> OfferEvent(const ElementFqId& element_fq_id) = 0;

    /// \brief Withdraws a previously offered service element.
    virtual void StopOfferEvent(const ElementFqId& element_fq_id) = 0;

    /// \brief Transmits one sample of the given service element.
    /// \param element_fq_id The service element the sample belongs to.
    /// \param data Pointer to the first byte of the sample. The transport must not retain this pointer beyond the
    ///        call, since the memory belongs to the sending event's slot storage.
    /// \param size Number of bytes to transmit.
    virtual Result<void> SendEvent(const ElementFqId& element_fq_id, const void* data, std::size_t size) = 0;

    /// \brief Notifies subscribers of the given service element without transmitting a new sample.
    virtual Result<void> NotifyEvent(const ElementFqId& element_fq_id) = 0;

    /// \brief Returns whether at least one consumer is currently subscribed to the given service element.
    virtual bool HasSubscribers(const ElementFqId& element_fq_id) const = 0;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_I_TRANSPORT_H
