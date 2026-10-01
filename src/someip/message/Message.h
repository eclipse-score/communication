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

#ifndef LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_H
#define LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_H

#include <message/MessageSD.h>
#include <message/MessageSOMEIP.h>
#include <message/MessagePayload.h>
#include <message/MessageBuilder.h>
#include <message/MessageComposer.h>

namespace lgsomeip {

using Message = MessageSOMEIP;
using Payload = MessagePayload;

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_MESSAGE_MESSAGE_H
