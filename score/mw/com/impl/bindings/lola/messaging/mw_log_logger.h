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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MW_LOG_LOGGER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MW_LOG_LOGGER_H

#include "score/message_passing/log/logging_callback.h"

namespace score::mw::com::impl::lola
{

/// \brief Creates an score::cpp::callback object that serves as a mw::log sink for message_passing logger
/// \details The score::cpp::callback is fed to the message_passing engine constructor. It is currently preconfigured
///          to send the messages to mw::log with "mp_2" context id, mapping message_passing severities
///          to mw::log loglevels in such a way that all the severities above Warning level are mapped
///          to the respecive log levels, and the rest is mapped to the Warning level.
///          This mapping is intended for Ticket-235378 investigation and is subject to change.
/// \return The created score::cpp::callback.
score::message_passing::LoggingCallback GetMwLogLogger();

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MW_LOG_LOGGER_H
