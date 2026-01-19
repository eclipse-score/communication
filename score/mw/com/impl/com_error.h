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
#ifndef SCORE_MW_COM_IMPL_COMERROR_H
#define SCORE_MW_COM_IMPL_COMERROR_H

#include "score/result/result.h"

namespace score::mw::com::impl
{
/**
 * \api
 * \brief Error codes for the mw::com API
 * \details Enum values map 1:1 to the Communication Management error codes.
 * \requirement SWS_CM_10432
 * \threadsafe
 */
enum class ComErrc : score::result::ErrorCode
{
    kServiceNotAvailable = 1,
    kMaxSamplesReached,
    kBindingFailure,
    kGrantEnforcementError,
    kPeerIsUnreachable,
    kFieldValueIsNotValid,
    kSetHandlerNotSet,
    kUnsetFailure,
    kSampleAllocationFailure,
    kIllegalUseOfAllocate,
    kServiceNotOffered,
    kCommunicationLinkError,
    kNoClients,
    kCommunicationStackError,
    kMaxSampleCountNotRealizable,
    kMaxSubscribersExceeded,
    kWrongMethodCallProcessingMode,
    kErroneousFileHandle,
    kCouldNotExecute,
    kInvalidInstanceIdentifierString,
    kInvalidBindingInformation,
    kEventNotExisting,
    kNotSubscribed,
    kInvalidConfiguration,
    kInvalidMetaModelShortname,
    kServiceInstanceAlreadyOffered,
    kCouldNotRestartProxy,
    kNotOffered,
    kInstanceIDCouldNotBeResolved,
    kFindServiceHandlerFailure,
    kInvalidHandle,
    kCallQueueFull,
    kServiceElementAlreadyExists,
};

/**
 * \api
 * \brief Error domain for mw::com (CommunicationManagement)
 * \details This class implements the ErrorDomain interface for the mw::com API.
 * \requirement SWS_CM_11329
 * \threadsafe
 */
// This switch-case statement implements a "mapping table". Length of the mapping table does not add complexity!
class ComErrorDomain final : public score::result::ErrorDomain
{
  public:
    // This lengthy switch-case leads to a coverity cyclomatic complexity warnings which we suppress. Rationale:
    // Switch-case statement is used for a simple error-code->error-message lookup. It does not add real complexity.
    // There is no other good solution for such a mapping, which is constexpr/compile time capable! Anything, which is
    // runtime, would need a complete refactoring of the public API or even the introduction of synchronization, which
    // is a no-go.
    // SCORE_CCM_NO_LINT
    /* Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there. */
    /// \todo Gcc compiler bug leads to a compiler warning if override is not added, even if final keyword is there
    /// (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78010). When bug is fixed, remove the override keyword from the
    /// MessageFor function signature and the AUTOSAR.MEMB.VIRTUAL.SPEC klocwork suppression.
    /**
     * \api
     * \brief Translates an error code into a human readable message
     * \details This function is required by the std::error_domain interface
     * \param code The error code to translates
     * \return returns a string view to the human readable message
     * \threadsafe
     */
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override final
    // Suppress "AUTOSAR C++14 A10-3-1" rule finding: Virtual function declaration shall contain exactly one of the
    // three specifiers: (1) virtual, (2) override, (3) final.
    // Rationale : See explanation above.
    // coverity[autosar_cpp14_a10_3_1_violation]
    {
        // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: "An unconditional throw or break
        // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed
        // switch statement." respectively. There is return for every switch case, adding a break would be dead code.
        // coverity[autosar_cpp14_m6_4_3_violation]
        switch (code)
        {
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kServiceNotAvailable):
                return "Service is not available.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kMaxSamplesReached):
                return "Application holds more SamplePtrs than commited in Subscribe().";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kBindingFailure):
                return "Local failure has been detected by the binding.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kGrantEnforcementError):
                return "Request was refused by Grant enforcement layer.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kPeerIsUnreachable):
                return "TLS handshake fail.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kFieldValueIsNotValid):
                return "Field Value is not valid.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kSetHandlerNotSet):
                return "SetHandler has not been registered.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kUnsetFailure):
                return "Failure has been detected by unset operation.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kSampleAllocationFailure):
                return "Not Sufficient memory resources can be allocated.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kIllegalUseOfAllocate):
                return "The allocation was illegally done via custom allocator (i.e., not via shared memory "
                       "allocation).";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kServiceNotOffered):
                return "Service not offered.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kCommunicationLinkError):
                return "Communication link is broken.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kNoClients):
                return "No clients connected.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kCommunicationStackError):
                return "Communication Stack Error, e.g. network stack, network binding, or communication framework "
                       "reports an error";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kMaxSampleCountNotRealizable):
                return "Provided maxSampleCount not realizable.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kMaxSubscribersExceeded):
                return "Subscriber count exceeded";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kWrongMethodCallProcessingMode):
                return "Wrong processing mode passed to constructor method call.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kErroneousFileHandle):
                return "The FileHandle returned from FindServce is corrupt/service not available.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kCouldNotExecute):
                return "Command could not be executed in provided Execution Context.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kInvalidInstanceIdentifierString):
                return "Invalid instance identifier format of string.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kInvalidBindingInformation):
                return "Internal error: Binding information invalid.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kEventNotExisting):
                return "Requested event does not exist on sender side.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kNotSubscribed):
                return "Request invalid: event proxy is not subscribed to the event.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kInvalidConfiguration):
                return "Invalid configuration.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kInvalidMetaModelShortname):
                return "Meta model short name does not adhere to naming requirements.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kServiceInstanceAlreadyOffered):
                return "Service instance is already offered";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kCouldNotRestartProxy):
                return "Could not recreate proxy after previous crash.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kNotOffered):
                return "Skeleton Event / Field has not been offered yet.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kInstanceIDCouldNotBeResolved):
                return "Runtime could not resolve a valid InstanceIdentifier from the provided InstanceSpecifier.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kFindServiceHandlerFailure):
                return "StartFindService failed to register handler.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kInvalidHandle):
                return "StopFindService was called with invalid FindServiceHandle.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kCallQueueFull):
                return "Call queue of service method is already full.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            case static_cast<score::result::ErrorCode>(ComErrc::kServiceElementAlreadyExists):
                return "A service element (event, field, method) with the same name already exists.";
            // coverity[autosar_cpp14_m6_4_5_violation]
            default:
                return "unknown future error";
        }
    }
};

score::result::Error MakeError(const ComErrc code, const std::string_view message = "");

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_COMERROR_H
