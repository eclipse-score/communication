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
#include "score/memory/shared/shared_memory_error.h"

#include <include/gtest/gtest.h>

namespace score::memory::shared
{
namespace
{

class SharedMemoryErrorTest : public ::testing::Test
{
  protected:
    void testErrorMessage(SharedMemoryErrorCode errorCode, std::string_view expectedErrorOutput)
    {
        const auto errorCodeTest =
            shared_memory_error_domain_dummy.MessageFor(static_cast<score::result::ErrorCode>(errorCode));
        ASSERT_EQ(errorCodeTest, expectedErrorOutput);
    }

    SharedMemoryErrorDomain shared_memory_error_domain_dummy{};
};

TEST_F(SharedMemoryErrorTest, UnknownSharedMemoryIdentifierError)
{
    testErrorMessage(SharedMemoryErrorCode::UnknownSharedMemoryIdentifier, "Unknown shared memory identifier");
}

TEST_F(SharedMemoryErrorTest, UnexpectedSharedMemoryIdentifierError)
{
    testErrorMessage(static_cast<SharedMemoryErrorCode>(100U), "unknown shared memory error");
}

}  // namespace
}  // namespace score::memory::shared
