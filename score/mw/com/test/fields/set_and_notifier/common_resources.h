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
#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_COMMON_RESOURCES_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_COMMON_RESOURCES_H

#include <string>

namespace score::mw::com::test
{

/// \brief Test scenarios supported by this test application.
enum class TestMode
{
    kNotifier,
    kSetAndNotifier,
    kGet,
    kGetAndNotifier,
    kSetAndGet,
    kSetAndGetAndNotifier,
};

struct TestConfig
{
    TestMode mode;
    std::string config_file_path;
};

/// \brief Parses the command line arguments for the test and returns a TestConfig object.
///
/// Terminates if the argument is not provided. We use this function instead of providing argc / argv directly to
/// InitializeRuntime so that we detect if the config file was not provided instead of silently falling back to the
/// default config. It also makes it easier to extend the command line arguments in the future if needed.
TestConfig ParseConfig(int argc, const char** argv);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_COMMON_RESOURCES_H
