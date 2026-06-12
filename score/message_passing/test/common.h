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
#ifndef SCORE_MESSAGE_PASSING_TEST_COMMON_H
#define SCORE_MESSAGE_PASSING_TEST_COMMON_H

#include <cstdint>

namespace score::message_passing::test
{

constexpr char kServiceIdentifier[]{"TestService"};
constexpr std::uint32_t kMaxSendSize{32U};
constexpr std::uint32_t kMaxReplySize{32U};
constexpr std::uint32_t kMaxNotifySize{32U};

}  // namespace score::message_passing::test

#endif  // SCORE_MESSAGE_PASSING_TEST_COMMON_H
