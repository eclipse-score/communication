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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_HANDLERBASE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_HANDLERBASE_H

#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/message_passing/i_receiver.h"

namespace score::mw::com::impl::lola
{
/// \brief Base class of all (message) handler used by MessagePassingFacade.
class HandlerBase
{
  public:
    HandlerBase() noexcept = default;

    virtual ~HandlerBase() noexcept = default;

    HandlerBase(HandlerBase&&) = delete;
    HandlerBase& operator=(HandlerBase&&) = delete;
    HandlerBase(const HandlerBase&) = delete;
    HandlerBase& operator=(const HandlerBase&) = delete;

    /// \brief Registers message received callbacks for messages handled by SubscribeEventHandler at _receiver_
    /// \param asil_level asil level of given _receiver_
    /// \param receiver receiver, where to register
    virtual void RegisterMessageReceivedCallbacks(const QualityType asil_level,
                                                  message_passing::IReceiver& receiver) noexcept = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_HANDLERBASE_H
