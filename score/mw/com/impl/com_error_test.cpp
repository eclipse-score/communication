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
#include "score/mw/com/impl/com_error.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

class ComErrorMessageForFixture : public ::testing::Test
{
  protected:
    void testErrorMessage(ComErrc errorCode, std::string_view expectedErrorOutput)
    {
        const auto errorCodeTest = ComErrorDomainDummy.MessageFor(static_cast<score::result::ErrorCode>(errorCode));
        ASSERT_EQ(errorCodeTest, expectedErrorOutput);
    }

    ComErrorDomain ComErrorDomainDummy{};
};

TEST_F(ComErrorMessageForFixture, MessageForServiceNotAvailable)
{
    testErrorMessage(ComErrc::kServiceNotAvailable, "Service is not available.");
}

TEST_F(ComErrorMessageForFixture, MessageForMaxSamplesReached)
{
    testErrorMessage(ComErrc::kMaxSamplesReached, "Application holds more SamplePtrs than commited in Subscribe().");
}

TEST_F(ComErrorMessageForFixture, MessageForBindingFailure)
{
    testErrorMessage(ComErrc::kBindingFailure, "Local failure has been detected by the binding.");
}

TEST_F(ComErrorMessageForFixture, MessageForGrantEnforcementError)
{
    testErrorMessage(ComErrc::kGrantEnforcementError, "Request was refused by Grant enforcement layer.");
}

TEST_F(ComErrorMessageForFixture, MessageForPeerIsUnreachable)
{
    testErrorMessage(ComErrc::kPeerIsUnreachable, "TLS handshake fail.");
}

TEST_F(ComErrorMessageForFixture, MessageForFieldValueIsNotValid)
{
    testErrorMessage(ComErrc::kFieldValueIsNotValid, "Field Value is not valid.");
}

TEST_F(ComErrorMessageForFixture, MessageForSetHandlerNotSet)
{
    testErrorMessage(ComErrc::kSetHandlerNotSet, "SetHandler has not been registered.");
}

TEST_F(ComErrorMessageForFixture, MessageForUnsetFailure)
{
    testErrorMessage(ComErrc::kUnsetFailure, "Failure has been detected by unset operation.");
}

TEST_F(ComErrorMessageForFixture, MessageForSampleAllocationFailure)
{
    testErrorMessage(ComErrc::kSampleAllocationFailure, "Not Sufficient memory resources can be allocated.");
}

TEST_F(ComErrorMessageForFixture, MessageForIllegalUseOfAllocate)
{
    testErrorMessage(
        ComErrc::kIllegalUseOfAllocate,
        "The allocation was illegally done via custom allocator (i.e., not via shared memory allocation).");
}

TEST_F(ComErrorMessageForFixture, MessageForServiceNotOffered)
{
    testErrorMessage(ComErrc::kServiceNotOffered, "Service not offered.");
}

TEST_F(ComErrorMessageForFixture, MessageForCommunicationLinkError)
{
    testErrorMessage(ComErrc::kCommunicationLinkError, "Communication link is broken.");
}

TEST_F(ComErrorMessageForFixture, MessageForNoClients)
{
    testErrorMessage(ComErrc::kNoClients, "No clients connected.");
}

TEST_F(ComErrorMessageForFixture, MessageForCommunicationStackError)
{
    testErrorMessage(
        ComErrc::kCommunicationStackError,
        "Communication Stack Error, e.g. network stack, network binding, or communication framework reports an error");
}

TEST_F(ComErrorMessageForFixture, MessageForMaxSampleCountNotRealizable)
{
    testErrorMessage(ComErrc::kMaxSampleCountNotRealizable, "Provided maxSampleCount not realizable.");
}

TEST_F(ComErrorMessageForFixture, MessageForMaxSubscribersExceeded)
{
    testErrorMessage(ComErrc::kMaxSubscribersExceeded, "Subscriber count exceeded");
}

TEST_F(ComErrorMessageForFixture, MessageForErroneousFileHandle)
{
    testErrorMessage(ComErrc::kErroneousFileHandle,
                     "The FileHandle returned from FindServce is corrupt/service not available.");
}

TEST_F(ComErrorMessageForFixture, MessageForCouldNotExecute)
{
    testErrorMessage(ComErrc::kCouldNotExecute, "Command could not be executed in provided Execution Context.");
}

TEST_F(ComErrorMessageForFixture, MessageForInvalidInstanceIdentifierString)
{
    testErrorMessage(ComErrc::kInvalidInstanceIdentifierString, "Invalid instance identifier format of string.");
}

TEST_F(ComErrorMessageForFixture, MessageForInvalidBindingInformation)
{
    testErrorMessage(ComErrc::kInvalidBindingInformation, "Internal error: Binding information invalid.");
}

TEST_F(ComErrorMessageForFixture, MessageForEventNotExisting)
{
    testErrorMessage(ComErrc::kEventNotExisting, "Requested event does not exist on sender side.");
}

TEST_F(ComErrorMessageForFixture, MessageForNotSubscribed)
{
    testErrorMessage(ComErrc::kNotSubscribed, "Request invalid: event proxy is not subscribed to the event.");
}

TEST_F(ComErrorMessageForFixture, MessageForInvalidConfiguration)
{
    testErrorMessage(ComErrc::kInvalidConfiguration, "Invalid configuration.");
}

TEST_F(ComErrorMessageForFixture, MessageForInvalidMetaModelShortname)
{
    testErrorMessage(ComErrc::kInvalidMetaModelShortname,
                     "Meta model short name does not adhere to naming requirements.");
}

TEST_F(ComErrorMessageForFixture, MessageForServiceInstanceAlreadyOffered)
{
    testErrorMessage(ComErrc::kServiceInstanceAlreadyOffered, "Service instance is already offered");
}

TEST_F(ComErrorMessageForFixture, MessageForCouldNotRestartProxy)
{
    testErrorMessage(ComErrc::kCouldNotRestartProxy, "Could not recreate proxy after previous crash.");
}

TEST_F(ComErrorMessageForFixture, MessageForNotOffered)
{
    testErrorMessage(ComErrc::kNotOffered, "Skeleton Event / Field has not been offered yet.");
}

TEST_F(ComErrorMessageForFixture, MessageForInstanceIDCouldNotBeResolved)
{
    testErrorMessage(ComErrc::kInstanceIDCouldNotBeResolved,
                     "Runtime could not resolve a valid InstanceIdentifier from the provided InstanceSpecifier.");
}

TEST_F(ComErrorMessageForFixture, MessageForFindServiceHandlerFailure)
{
    testErrorMessage(ComErrc::kFindServiceHandlerFailure, "StartFindService failed to register handler.");
}

TEST_F(ComErrorMessageForFixture, MessageForInvalidHandleFailure)
{
    testErrorMessage(ComErrc::kInvalidHandle, "StopFindService was called with invalid FindServiceHandle.");
}

TEST_F(ComErrorMessageForFixture, MessageForCallQueueFull)
{
    testErrorMessage(ComErrc::kCallQueueFull, "Call queue of service method is already full.");
}

using ComErrorMessageForDeathTest = ComErrorMessageForFixture;
TEST_F(ComErrorMessageForDeathTest, MessageForkInvalidTerminates)
{
    // When calling MessageFor with the code kInvalid
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = ComErrorDomainDummy.MessageFor(static_cast<score::result::ErrorCode>(ComErrc::kInvalid)),
                 ".*");
}

TEST_F(ComErrorMessageForDeathTest, MessageForNumEnumElements)
{
    // When calling MessageFor with the code kNumEnumElements
    // Then the program terminates
    EXPECT_DEATH(
        score::cpp::ignore = ComErrorDomainDummy.MessageFor(static_cast<score::result::ErrorCode>(ComErrc::kNumEnumElements)),
        ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
