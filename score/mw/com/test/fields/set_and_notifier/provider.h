/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_PROVIDER_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_PROVIDER_H

#include <score/stop_token.hpp>

#include <optional>
#include <string_view>

namespace score::mw::com::test
{

/// \brief Provider scenarios supported by this test application.
enum class ProviderMode
{
    kNotifier,
    kSetAndNotifier,
};

/// \brief Parses the mode string (as provided on the command line) into a ProviderMode.
/// \return The corresponding ProviderMode, or std::nullopt if the string is not a recognized mode.
std::optional<ProviderMode> ParseProviderMode(std::string_view mode);

void run_provider(const score::cpp::stop_token& stop_token, ProviderMode mode);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_PROVIDER_H
