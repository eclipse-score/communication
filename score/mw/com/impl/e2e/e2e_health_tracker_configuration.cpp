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
#include "score/mw/com/impl/e2e/e2e_health_tracker_configuration.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::e2e
{

HealthTrackerConfiguration::HealthTrackerConfiguration(const std::uint8_t error_threshold,
                                                       const std::uint8_t recovery_threshold,
                                                       const std::uint8_t window_size)
    : error_threshold_{error_threshold}, recovery_threshold_{recovery_threshold}, window_size_{window_size}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(error_threshold >= 1U, "error_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(recovery_threshold >= 1U,
                                                      "recovery_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(window_size >= error_threshold,
                                                      "window_size must be at least error_threshold");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(window_size >= recovery_threshold,
                                                      "window_size must be at least recovery_threshold");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(window_size <= kMaxHealthWindowSize,
                                                      "window_size must not exceed kMaxHealthWindowSize");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        (error_threshold + recovery_threshold) > window_size,
        "error_threshold + recovery_threshold must exceed window_size to avoid oscillation between kOk and kError");
}

}  // namespace score::mw::com::impl::e2e
