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

#ifndef LG_SOMEIP_EXCEPTION_EXCEPTION_H
#define LG_SOMEIP_EXCEPTION_EXCEPTION_H

// SOMEIP Exception Module
#include "ApplicationError.h"
#include "SubscribeError.h"
#include "MessageError.h"
#include "RuntimeError.h"
#include "ConfigurationError.h"

namespace lgsomeip {

#define LSAR_APPLICATION_ERROR(code) ApplicationErrorException(code, __FILE__, __LINE__)
#define LSAR_CONFIGURATION_ERROR(msg) ConfigurationErrorException(msg, __FILE__, __LINE__)
#define LSAR_SUBSCRIBE_ERROR(msg) SubscribeErrorException(msg, __FILE__, __LINE__)
#define LSAR_MESSAGE_ERROR(msg) MessageErrorException(msg, __FILE__, __LINE__)
#define LSAR_RUNTIME_ERROR(msg) RuntimeErrorException(msg, __FILE__, __LINE__)

} // namespace lgsomeip

#endif //  LG_SOMEIP_EXCEPTION_EXCEPTION_H
