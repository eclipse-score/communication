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
#include "score/mw/com/impl/configuration/configuration_error.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

class ConfigurationErrorTest : public ::testing::Test
{
  protected:
    void testErrorMessage(configuration_errc errorCode, std::string_view expectedErrorOutput)
    {
        const auto errorCodeTest =
            ConfigurationErrorDomainDummy.MessageFor(static_cast<score::result::ErrorCode>(errorCode));
        ASSERT_EQ(errorCodeTest, expectedErrorOutput);
    }

    ConfigurationErrorDomain ConfigurationErrorDomainDummy{};
};

TEST_F(ConfigurationErrorTest, MessageForSerializationDeploymentinformationInvalid)
{
    testErrorMessage(configuration_errc::serialization_deploymentinformation_invalid,
                     "serialization of <DeploymentInformation> is invalid");
}

TEST_F(ConfigurationErrorTest, MessageForSerializationNoShmbindinginformation)
{
    testErrorMessage(configuration_errc::serialization_no_shmbindinginformation,
                     "no serialization of <LoLaShmBindingInfo>");
}

TEST_F(ConfigurationErrorTest, MessageForSerializationShmbindinginformationInvalid)
{
    testErrorMessage(configuration_errc::serialization_shmbindinginformation_invalid,
                     "serialization of <LoLaShmBindingInfo> is invalid");
}

TEST_F(ConfigurationErrorTest, MessageForSerializationSomeipbindinginformationInvalid)
{
    testErrorMessage(configuration_errc::serialization_someipbindinginformation_invalid,
                     "serialization of <SomeIpBindingInfo> is invalid");
}

TEST_F(ConfigurationErrorTest, MessageForSerializationNoSomeipbindinginformationInvalid)
{
    testErrorMessage(configuration_errc::serialization_no_someipbindinginformation,
                     "no serialization of <SomeIpBindingInfo>");
}

TEST_F(ConfigurationErrorTest, MessageForDefault)
{
    testErrorMessage(static_cast<configuration_errc>(-1), "unknown configuration error");
}

TEST(ConfigurationError, MakeErrorExpectedError)
{
    const score::result::Error err{MakeError(configuration_errc::serialization_deploymentinformation_invalid, "")};
    EXPECT_EQ(*err, static_cast<int>(configuration_errc::serialization_deploymentinformation_invalid));
    EXPECT_TRUE(err.UserMessage().empty());
}

}  // namespace
}  // namespace score::mw::com::impl
