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

#include "score/mw/com/impl/bindings/lola/messaging/notify_event_handler.h"

#include "score/memory/shared/offset_ptr.h"
#include "score/os/unistd.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <exception>
#include <set>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl::lola
{

/// \brief Determines the unique identifier for this application instance.
/// \details This function implements the logic to select the application identifier. It prioritizes the
///          explicitly configured 'applicationID' from the global configuration. If that is not present,
///          it falls back to using the process's real user ID (uid) as the identifier.
/// \param config The application's configuration object.
/// \return The determined application identifier (either the configured ID or the process UID).
    
uid_t Runtime::DetermineApplicationIdentifier(const Configuration& config) const noexcept
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
        return uid_;
    }
}

Runtime::Runtime(const Configuration& config,
                 concurrency::Executor& long_running_threads,
                 std::unique_ptr<lola::tracing::TracingRuntime> lola_tracing_runtime)
    : IRuntime{},
      configuration_{config},
      long_running_threads_{long_running_threads},
      lola_message_passing_control_{Runtime::HasAsilBSupport(),
                                    config.GetGlobalConfiguration().GetSenderMessageQueueSize()},
      lola_messaging_stop_source_{},
      lola_messaging_{lola_messaging_stop_source_,
                      // LCOV_EXCL_START (Tool incorrectly marks this line as not covered. However, the lines before and
                      // after are covered so this is clearly an error by the tool. Suppression can be removed when bug
                      // is fixed in Ticket-184253).
                      std::make_unique<NotifyEventHandler>(lola_message_passing_control_,
                                                           Runtime::HasAsilBSupport(),
                                                           lola_messaging_stop_source_.get_token()),
                      lola_message_passing_control_,
                      // Suppress "AUTOSAR C++14 M12-1-1", The rule states: "An object’s dynamic type shall not be used
                      // from the body of its constructor or destructor".
                      // This is false positive, GetMessagePassingCfg is not a virtual function.
                      // coverity[autosar_cpp14_m12_1_1_violation]
                      Runtime::GetMessagePassingCfg(QualityType::kASIL_QM),
                      // LCOV_EXCL_STOP
                      Runtime::HasAsilBSupport()
                          // coverity[autosar_cpp14_m12_1_1_violation]
                          ? score::cpp::optional<MessagePassingFacade::AsilSpecificCfg>{Runtime::GetMessagePassingCfg(
                                QualityType::kASIL_B)}
                          : score::cpp::nullopt},
      service_discovery_client_{long_running_threads_},
      tracing_runtime_{std::move(lola_tracing_runtime)},
      rollback_data_{},
      pid_{os::Unistd::instance().getpid()},
      uid_{os::Unistd::instance().getuid()},
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

IMessagePassingService& Runtime::GetLolaMessaging() noexcept
{
    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // To avoid overhead such as shared_ptr in the result, a non-const reference to the instance is returned instead.
    // This instance exposes another sub-API that can change the its state and therefore also the state of instance
    // holder. API callers get the reference and use it in place without leaving the scope, so the reference remains
    // valid.
    // coverity[autosar_cpp14_a9_3_1_violation]
    return lola_messaging_;
}

bool Runtime::HasAsilBSupport() const noexcept
{
    return configuration_.GetGlobalConfiguration().GetProcessAsilLevel() == QualityType::kASIL_B;
}

impl::tracing::ITracingRuntimeBinding* Runtime::GetTracingRuntime() noexcept
{
    return tracing_runtime_.get();
}

MessagePassingFacade::AsilSpecificCfg Runtime::GetMessagePassingCfg(const QualityType asil_level) const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(((asil_level == QualityType::kASIL_B) || (asil_level == QualityType::kASIL_QM)),
                           "Asil level must be asil_qm or asil_b.");
    if ((!HasAsilBSupport()) && (asil_level == QualityType::kASIL_B))
    {
        score::mw::log::LogFatal("lola")
            << __func__ << __LINE__
            << "Invalid call to GetMessagePassingCfg with asil_level B although app/process is configured for QM only.";
        /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
        std::terminate();
    }
    std::set<uid_t> aggregated_allowed_users;

    for (const auto& instanceDeplElement : configuration_.GetServiceInstances())
    {
        const auto* const instance_deployment =
            std::get_if<LolaServiceInstanceDeployment>(&instanceDeplElement.second.bindingInfo_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_deployment != nullptr,
                               "Instance deployment must contain Lola binding in order to create a lola runtime!");
        if (AggregateAllowedUsers(aggregated_allowed_users, instance_deployment->allowed_consumer_, asil_level))
        {
            break;
        }
        if (AggregateAllowedUsers(aggregated_allowed_users, instance_deployment->allowed_provider_, asil_level))
        {
            break;
        }
    }

    return {configuration_.GetGlobalConfiguration().GetReceiverMessageQueueSize(asil_level),
            std::vector<uid_t>(aggregated_allowed_users.begin(), aggregated_allowed_users.end())};
}

bool Runtime::AggregateAllowedUsers(std::set<uid_t>& aggregated_allowed_users,
                                    const std::unordered_map<QualityType, std::vector<uid_t>>& allowed_user_ids,
                                    const QualityType asil_level)
{
    const auto user_ids = allowed_user_ids.find(asil_level);
    if (user_ids != allowed_user_ids.cend())
    {
        if (user_ids->second.empty())
        {
            aggregated_allowed_users.clear();
            return true;
        }
        for (const auto& user_identifier : user_ids->second)
        {
            // result of insert can be ignored, because at the end the element is in
            // the set and we have no expectation, if it already was in the set before or not.
            score::cpp::ignore = aggregated_allowed_users.insert(user_identifier);
        }
    }
    return false;
}

ShmSizeCalculationMode Runtime::GetShmSizeCalculationMode() const noexcept
{
    return configuration_.GetGlobalConfiguration().GetShmSizeCalcMode();
}

IServiceDiscoveryClient& Runtime::GetServiceDiscoveryClient() noexcept
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

RollbackSynchronization& Runtime::GetRollbackSynchronization() noexcept
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

uid_t Runtime::GetUid() const noexcept
{
    return uid_;
}

uid_t Runtime::GetApplicationId() const noexcept
{
    return application_id_;
}

}  // namespace score::mw::com::impl::lola
