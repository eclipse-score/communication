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

#ifndef SCORE_SOCOM_REFERENCE_TOKEN_HPP
#define SCORE_SOCOM_REFERENCE_TOKEN_HPP

#include <memory>

namespace score::socom {

class Final_action;
class Deadlock_detector;

using Reference_token = std::shared_ptr<Final_action>;
using Weak_reference_token = std::weak_ptr<Final_action>;

}  // namespace score::socom

#endif  // SCORE_SOCOM_REFERENCE_TOKEN_HPP
