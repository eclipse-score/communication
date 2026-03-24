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
#include "score/mw/com/mocking/runtime_mock.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/mocking/test_type_utilities.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"

#include "score/filesystem/path.h"
#include "score/string/string_literal.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

namespace score::mw::com::runtime
{
namespace
{

using namespace ::testing;

const auto kDummyInstanceSpecifier = impl::InstanceSpecifier::Create(std::string{"DummyInstanceSpecifier"}).value();
const filesystem::Path kDummyConfigurationPath{"/my/dummy/path"};

class RuntimeMockFixture : public ::testing::Test
{
  public:
    RuntimeMockFixture()
    {
        InjectRuntimeMockImpl(runtime_mock_);
    }

    InstanceIdentifierContainer dummy_instance_identifier_container_{impl::MakeFakeInstanceIdentifier(1U),
                                                                     impl::MakeFakeInstanceIdentifier(1U)};

    RuntimeMock runtime_mock_{};
};

TEST_F(RuntimeMockFixture, ResolveInstanceIdsDispatchesToMockAfterInjectingMock)
{
    // Given that a mocked runtime has been injected

    // Expecting that ResolveInstanceIds will be called on the mock which returns a valid InstanceIdentifierContainer
    EXPECT_CALL(runtime_mock_, ResolveInstanceIDs(kDummyInstanceSpecifier))
        .WillOnce(Return(dummy_instance_identifier_container_));

    // When calling ResolveInstanceIds
    auto resolve_instance_ids_result = ResolveInstanceIDs(kDummyInstanceSpecifier);

    // Then the result contains the same InstanceIdentifierContainer that was returned by the mock
    ASSERT_TRUE(resolve_instance_ids_result.has_value());
    EXPECT_EQ(resolve_instance_ids_result, dummy_instance_identifier_container_);
}

TEST_F(RuntimeMockFixture, ResolveInstanceIdsReturnsErrorWhenMockReturnsError)
{
    // Given that a mocked runtime has been injected

    // Expecting that ResolveInstanceIds will be called on the mock which returns an error
    const auto error_code = impl::ComErrc::kCouldNotExecute;
    EXPECT_CALL(runtime_mock_, ResolveInstanceIDs(kDummyInstanceSpecifier))
        .WillOnce(Return(MakeUnexpected(error_code)));

    // When calling ResolveInstanceIds
    auto resolve_instance_ids_result = ResolveInstanceIDs(kDummyInstanceSpecifier);

    // Then the result contains the same error that was returned by the mock
    ASSERT_FALSE(resolve_instance_ids_result.has_value());
    EXPECT_EQ(resolve_instance_ids_result.error(), error_code);
}

TEST_F(RuntimeMockFixture, InitializeRuntimeDispatchesToMockAfterInjectingMock)
{
    // Given that a mocked runtime has been injected

    // Expecting that InitializeRuntime will be called on the mock with the same argc / argv that is passed to
    // InitializeRuntime (where argv has been converted to an score::cpp::span
    constexpr std::int32_t argc{1U};
    score::StringLiteral argv[] = {"some_argument"};
    EXPECT_CALL(runtime_mock_, InitializeRuntime(argc, _)).WillOnce(Invoke([argv](auto, auto argv_span) {
        const score::cpp::span<const score::StringLiteral> expected_argv_span(
            argv, static_cast<score::cpp::span<const score::StringLiteral>::size_type>(argc));
        ASSERT_EQ(argv_span.size(), expected_argv_span.size());
        for (std::size_t i = 0; i < argv_span.size(); ++i)
        {
            EXPECT_EQ(argv_span.data()[i], expected_argv_span.data()[i]);
        }
    }));

    // When calling InitializeRuntime
    InitializeRuntime(argc, argv);
}

TEST_F(RuntimeMockFixture, InitializeRuntimeWithRuntimeConfigurationDispatchesToMockAfterInjectingMock)
{
    // Given that a mocked runtime has been injected

    // Expecting that InitializeRuntime will be called on the mock
    RuntimeConfiguration runtime_configuration{kDummyConfigurationPath};
    EXPECT_CALL(runtime_mock_, InitializeRuntime(_)).WillOnce(Invoke([](auto& runtime_configuration) {
        EXPECT_EQ(runtime_configuration.GetConfigurationPath(), kDummyConfigurationPath);
    }));

    // When calling InitializeRuntime
    InitializeRuntime(runtime_configuration);
}

}  // namespace
}  // namespace score::mw::com::runtime
