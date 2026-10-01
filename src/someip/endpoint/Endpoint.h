/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_ENDPOINT_ENDPOINT_H
#define LG_SOMEIP_ENDPOINT_ENDPOINT_H

#include <endpoint/EndpointBase.h>
#include <endpoint/EndpointTCP.h>
#include <endpoint/EndpointUDP.h>

#if defined(ENABLE_QNX_MESSAGE_PASSING)
#include <endpoint/EndpointMessagePassing.h>
#endif // ENABLE_QNX_MESSAGE_PASSING

#endif // LG_SOMEIP_ENDPOINT_ENDPOINT_H
