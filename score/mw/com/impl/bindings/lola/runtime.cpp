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
#include "score/mw/com/impl/bindings/lola/runtime.h"

#include "score/memory/shared/offset_ptr.h"
#include "score/mw/log/logging.h"
#include "score/os/unistd.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <exception>
#include <set>
#include <unordered_map>
#include <vector>

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_factory.h"

namespace score::mw::com::impl::lola
{

/// \brief Determines the unique identifier for this application instance.
/// \details This function implements the logic to select the application identifier. It prioritizes the
///          explicitly configured 'applicationID' from the global configuration. If that is not present,
///          it falls back to using the process's real user ID (uid) as the identifier.
/// \param config The application's configuration object.
/// \return The determined application identifier (either the configured ID or the process UID).

std::uint32_t Runtime::DetermineApplicationIdentifier(const Configuration& config) const noexcept
{
    const auto& global_config = config.GetGlobalConfiguration();
    const auto application_id = global_config.GetApplicationId();
    if (application_id.has_value())
    {
        return application_id.value();
    }
    else
    {
        score::mw::log::LogInfo("lola") << "No explicit applicationID configured. Falling back to using process UID. "
                                        << "Ensure unique UIDs for applications using mw::com.";
        // The uid_t is only used internally (in the fallback case) and then casted to an std::uint32_t
        static_assert(sizeof(uid_t) <= 4, "For more than 32 bits we cannot guarantee the key to be unique");
        return static_cast<std::uint32_t>(os::Unistd::instance().getuid());
    }
}

Runtime::Runtime(const Configuration& config,
                 concurrency::Executor& long_running_threads,
                 std::unique_ptr<lola::tracing::TracingRuntime> lola_tracing_runtime)
    : IRuntime{},
      configuration_{config},
      long_running_threads_{long_running_threads},
      lola_messaging_stop_source_{},
      lola_messaging_service_{// LCOV_EXCL_START Tooling issue - Lines before and after are covered Ticket-184253
                              Runtime::GetMessagePassingCfg(QualityType::kASIL_QM),
                              // LCOV_EXCL_STOP
                              Runtime::HasAsilBSupport()
                                  ? std::optional<AsilSpecificCfg>{Runtime::GetMessagePassingCfg(QualityType::kASIL_B)}
                                  : std::nullopt,
                              std::make_unique<MessagePassingServiceInstanceFactory>()},
      service_discovery_client_{long_running_threads_},
      tracing_runtime_{std::move(lola_tracing_runtime)},
      rollback_data_{},
      pid_{os::Unistd::instance().getpid()},
      application_id_{DetermineApplicationIdentifier(config)}
{
    // At this stage we know/can decide, whether we are an ASIL-B or ASIL-QM application. OffsetPtr bounds-checking
    // is costly and is only done in case we are an ASIL-B app!
    score::cpp::ignore = score::memory::shared::EnableOffsetPtrBoundsChecking(Runtime::HasAsilBSupport());
}

BindingType Runtime::GetBindingType() const noexcept
{
    return BindingType::kLoLa;
}

IMessagePassingService& Runtime::GetLolaMessaging() & noexcept
{
    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // To avoid overhead such as shared_ptr in the result, a non-const reference to the instance is returned instead.
    // This instance exposes another sub-API that can change the its state and therefore also the state of instance
    // holder. API callers get the reference and use it in place without leaving the scope, so the reference remains
    // valid.
    // coverity[autosar_cpp14_a9_3_1_violation]
    return lola_messaging_service_;
}

bool Runtime::HasAsilBSupport() const noexcept
{
    return configuration_.GetGlobalConfiguration().GetProcessAsilLevel() == QualityType::kASIL_B;
}

impl::tracing::IBindingTracingRuntime* Runtime::GetTracingRuntime() noexcept
{
    return tracing_runtime_.get();
}

AsilSpecificCfg Runtime::GetMessagePassingCfg(const QualityType asil_level) const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        ((asil_level == QualityType::kASIL_B) || (asil_level == QualityType::kASIL_QM)),
        "Asil level must be asil_qm or asil_b.");
    if ((!HasAsilBSupport()) && (asil_level == QualityType::kASIL_B))
    {
        score::mw::log::LogFatal("lola")
            << __func__ << __LINE__
            << "Invalid call to GetMessagePassingCfg with asil_level B although app/process is configured for QM only.";
        std::terminate();
    }
    std::set<uid_t> aggregated_allowed_users = configuration_.GetAggregatedAllowedUsers(asil_level);

    return {configuration_.GetGlobalConfiguration().GetReceiverMessageQueueSize(asil_level),
            std::vector<uid_t>(aggregated_allowed_users.begin(), aggregated_allowed_users.end())};
}

ShmSizeCalculationMode Runtime::GetShmSizeCalculationMode() const noexcept
{
    return configuration_.GetGlobalConfiguration().GetShmSizeCalcMode();
}

IServiceDiscoveryClient& Runtime::GetServiceDiscoveryClient() & noexcept
{
    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // To avoid overhead such as shared_ptr in the result, a non-const reference to the instance is returned instead.
    // This instance exposes another sub-API that can change the its state and therefore also the state of instance
    // holder. API callers get the reference and use it in place without leaving the scope, so the reference remains
    // valid.
    // coverity[autosar_cpp14_a9_3_1_violation]
    return service_discovery_client_;
}

RollbackSynchronization& Runtime::GetRollbackSynchronization() & noexcept
{
    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // To avoid overhead such as shared_ptr in the result, a non-const reference to the instance of data is returned
    // instead. API callers get the reference and use it in place without leaving the scope, so the reference remains
    // valid.
    // coverity[autosar_cpp14_a9_3_1_violation]
    return rollback_data_;
}

pid_t Runtime::GetPid() const noexcept
{
    return pid_;
}

GlobalConfiguration::ApplicationId Runtime::GetApplicationId() const noexcept
{
    return application_id_;
}

}  // namespace score::mw::com::impl::lola
