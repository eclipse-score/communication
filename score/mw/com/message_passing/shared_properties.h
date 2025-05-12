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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SHARED_PROPERTIES_H
#define SCORE_MW_COM_MESSAGE_PASSING_SHARED_PROPERTIES_H

#include "score/mw/com/message_passing/message.h"
#include <score/callback.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>

namespace score::mw::com::message_passing
{

using LogFunction = score::cpp::callback<void(std::ostream&), 128UL>;

using LoggingCallback = score::cpp::callback<void(const LogFunction)>;

void DefaultLoggingCallback(const LogFunction);

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SHARED_PROPERTIES_H
