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
#ifndef SCORE_LIB_MESSAGE_PASSING_SERVER_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_SERVER_FACTORY_H

#ifdef __QNX__
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"
#else
#include "score/message_passing/unix_domain/unix_domain_server_factory.h"
#endif

namespace score::message_passing
{

// Suppress "AUTOSAR C++14 A16-0-1" rule findings.
// This is the standard way to determine if it runs on QNX or Unix
// coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNX__
using Engine = score::message_passing::QnxDispatchEngine;
using ServerFactory = score::message_passing::QnxDispatchServerFactory;
// coverity[autosar_cpp14_a16_0_1_violation]
#else
using Engine = score::message_passing::UnixDomainEngine;
using ServerFactory = score::message_passing::UnixDomainServerFactory;
// coverity[autosar_cpp14_a16_0_1_violation]
#endif

}  // namespace score::message_passing

#endif  // SCORE_LIB_MESSAGE_PASSING_SERVER_FACTORY_H
