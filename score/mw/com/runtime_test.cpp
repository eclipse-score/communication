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
#include "score/mw/com/runtime.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"

#include "score/mw/com/types.h"
#include <score/jthread.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <thread>

#include <fstream>
#include <iostream>

namespace score::mw::com::runtime
{
namespace
{

using ::testing::Contains;
using ::testing::Return;

const auto kServiceSpecifierString = "abc/abc/TirePressurePort";
const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{kServiceSpecifierString}).value();
const auto kServiceIdentifier = impl::make_ServiceIdentifierType("foo", 13, 37);
impl::LolaServiceInstanceId kLolaInstanceId{23U};
impl::ServiceInstanceId kInstanceId{kLolaInstanceId};
const impl::ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                      impl::LolaServiceInstanceDeployment{kLolaInstanceId},
                                                      impl::QualityType::kASIL_QM,
                                                      kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const impl::ServiceTypeDeployment kTypeDeployment{impl::LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class RuntimeTestFixture : public ::testing::Test
{
  public:
    impl::RuntimeMockGuard runtime_mock_guard_{};
};

class RuntimeTestExternalJsonFixture : public ::testing::Test
{
  public:
    const std::string get_path(const std::string& file_name)
    {
        const std::string default_path = "score/mw/com/impl/configuration/example/" + file_name;

        std::ifstream file(default_path);
        if (file.is_open())
        {
            file.close();
            return default_path;
        }
        else
        {
            return "external/safe_posix_platform/" + default_path;
        }
    }
};

TEST_F(RuntimeTestFixture, ResolveInstanceIdsWillDispatchToRuntimeSingleton)
{
    RecordProperty("Verifies", "SCR-21781439");
    RecordProperty("Description", "Checks that ResolveInstanceIds will call resolve on the Runtime singleton.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the resolution request is forwarded to the Runtime singleton
    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier));

    // When trying to resolve an instance specifier
    score::cpp::ignore = score::mw::com::runtime::ResolveInstanceIDs(kInstanceSpecifier);
}

TEST_F(RuntimeTestFixture, ResolveInstanceIdsWillReturnInstanceIdentifierContainerFromRuntimeOnSuccess)
{
    RecordProperty("Verifies", "SCR-21781439");
    RecordProperty(
        "Description",
        "Checks that ResolveInstanceIds will return the InstanceIdentifierContainer from the Runtime singleton.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the resolution request is forwarded
    ON_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier))
        .WillByDefault(Return(InstanceIdentifierContainer{kInstanceIdWithLolaBinding}));

    // When trying to resolve an instance specifier
    auto instance_identifiers_result = score::mw::com::runtime::ResolveInstanceIDs(kInstanceSpecifier);

    // Then the result will contain the instance identifier returned by the Runtime singleton
    ASSERT_TRUE(instance_identifiers_result.has_value());
    ASSERT_THAT(instance_identifiers_result.value(), Contains(kInstanceIdWithLolaBinding));
}

TEST_F(RuntimeTestFixture, ResolveInstanceIdsWillPropagateErrorFromRuntime)
{
    RecordProperty("Verifies", "SCR-21781439");
    RecordProperty(
        "Description",
        "Checks that ResolveInstanceIds will return kInstanceIDCouldNotBeResolved if the Runtime singleton returns an "
        "empty vector.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Expecting that the resolution request is forwarded and the Runtime singleton returns an error
    ON_CALL(runtime_mock_guard_.runtime_mock_, resolve(kInstanceSpecifier))
        .WillByDefault(Return(InstanceIdentifierContainer{}));

    // When trying to resolve an instance specifier
    auto instance_identifiers_result = score::mw::com::runtime::ResolveInstanceIDs(kInstanceSpecifier);

    // Then the result will contain an error with the value kInstanceIDCouldNotBeResolved
    ASSERT_FALSE(instance_identifiers_result.has_value());
    ASSERT_EQ(instance_identifiers_result.error(), impl::ComErrc::kInstanceIDCouldNotBeResolved);
}

TEST_F(RuntimeTestExternalJsonFixture, ResolveInstanceIdsIsThreadSafe)
{
    RecordProperty("Verifies", "SCR-21781439");
    RecordProperty("Description", "Checks that ResolveInstanceIds is thread safe.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    constexpr std::size_t number_of_threads{10U};
    constexpr std::size_t number_of_calls_per_thread{10U};

    // Given a runtime initialised with the path to a configuration file
    auto json_path = get_path("mw_com_config.json");
    score::StringLiteral test_args[] = {"dummyname", "-service_instance_manifest", json_path.c_str()};
    const std::int32_t num_test_args{3};
    InitializeRuntime(num_test_args, test_args);

    // and an instance identifier and InstanceSpecifier derived from the configuration
    const auto instance_specifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    auto instance_identifiers_result = score::mw::com::runtime::ResolveInstanceIDs(instance_specifier);
    ASSERT_TRUE(instance_identifiers_result.has_value());
    ASSERT_EQ(instance_identifiers_result.value().size(), 1);
    const auto& expected_instance_identifier = instance_identifiers_result.value()[0];

    // When trying to resolve the instance specifier concurrently
    std::vector<score::cpp::jthread> threads(number_of_threads);
    std::for_each(threads.begin(), threads.end(), [&expected_instance_identifier](auto& thread) {
        thread = score::cpp::jthread{[&expected_instance_identifier]() {
            for (std::size_t j = 0; j < number_of_calls_per_thread; ++j)
            {
                // Then the returned instance identifier should always be the same
                auto actual_instance_identifiers_result = score::mw::com::runtime::ResolveInstanceIDs(kInstanceSpecifier);
                ASSERT_TRUE(actual_instance_identifiers_result.has_value());
                ASSERT_EQ(actual_instance_identifiers_result.value().size(), 1);
                EXPECT_TRUE(actual_instance_identifiers_result.value()[0] == expected_instance_identifier);
            }
        }};
    });
}

}  // namespace
}  // namespace score::mw::com::runtime
