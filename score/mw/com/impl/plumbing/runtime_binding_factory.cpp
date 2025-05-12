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
#include "score/mw/com/impl/plumbing/runtime_binding_factory.h"

#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"

#include "score/mw/com/impl/bindings/lola/runtime.h"
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"

#include <score/overload.hpp>
#include <score/utility.hpp>

#include <unordered_map>
#include <utility>

std::unordered_map<score::mw::com::impl::BindingType, std::unique_ptr<score::mw::com::impl::IRuntimeBinding>>
score::mw::com::impl::RuntimeBindingFactory::CreateBindingRuntimes(
    Configuration& configuration,
    concurrency::Executor& long_running_threads,
    const score::cpp::optional<tracing::TracingFilterConfig>& tracing_filter_config)
{
    std::unordered_map<BindingType, std::unique_ptr<IRuntimeBinding>> result{};

    // Iterate through all the service types we have to find out, which technical bindings are used.
    for (auto it = configuration.GetServiceTypes().cbegin(); it != configuration.GetServiceTypes().cend(); ++it)
    {
        auto service_type = *it;

        auto deployment_info_visitor = score::cpp::overload(
            [&result, &configuration, &long_running_threads, &tracing_filter_config](const LolaServiceTypeDeployment&) {
                std::unique_ptr<lola::tracing::TracingRuntime> lola_tracing_runtime{nullptr};
                if (configuration.GetTracingConfiguration().IsTracingEnabled() && tracing_filter_config.has_value())
                {
                    const auto number_of_needed_tracing_slots =
                        tracing_filter_config->GetNumberOfTracingSlots(configuration);
                    lola_tracing_runtime =
                        std::make_unique<lola::tracing::TracingRuntime>(number_of_needed_tracing_slots, configuration);
                }

                auto lola_runtime = std::make_unique<score::mw::com::impl::lola::Runtime>(
                    configuration, long_running_threads, std::move(lola_tracing_runtime));
                const auto pair = result.emplace(std::make_pair(BindingType::kLoLa, std::move(lola_runtime)));
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(pair.second, "Failed to emplace lola runtime binding");
                return true;
            },
            // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
            // expression statement and identifier declaration shall be placed on a
            // separate line.". Following line statement is fine, this happens due to
            // clang formatting.
            // coverity[autosar_cpp14_a7_1_7_violation]
            [](const score::cpp::blank&) noexcept {
                return false;
            });
        const bool runtime_created = std::visit(deployment_info_visitor, service_type.second.binding_info_);

        // LCOV_EXCL_BR_START: Since we currently only support a single binding, we immediately stop further runtime
        // creation after the first is created. score::cpp::blank{} is not used in production code and the overload for
        // LolaServiceTypeDeployment cannot return false.
        if (runtime_created)
        {
            break;
        }
        // LCOV_EXCL_BR_STOP
    }
    return result;
}
