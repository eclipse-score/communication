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
#include "score/mw/com/impl/plumbing/binding_runtime_factory.h"

#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"

#include "score/mw/com/impl/bindings/lola/runtime.h"
#include "score/mw/com/impl/bindings/lola/tracing/tracing_runtime.h"

#include <score/overload.hpp>
#include <score/utility.hpp>

#include <unordered_map>
#include <utility>

std::unordered_map<score::mw::com::impl::BindingType, std::unique_ptr<score::mw::com::impl::IBindingRuntime>>
score::mw::com::impl::BindingRuntimeFactory::CreateBindingRuntimes(
    Configuration& configuration,
    concurrency::Executor& long_running_threads,
    const std::optional<tracing::TracingFilterConfig>& tracing_filter_config)
{
    std::unordered_map<BindingType, std::unique_ptr<IBindingRuntime>> result{};

    // At the moment we are only interested in LoLa bindings and therefore only create a LoLa runtime.
    // If other bindings will be relevant, the Configuration class needs to be extended to enable checking
    // for services of this type and the related runtime needs to be created here.
    const auto configuration_has_lola_services = configuration.HasLolaServiceDeployment();

    if (configuration_has_lola_services.has_value() && configuration_has_lola_services.value())
    {
        std::unique_ptr<lola::tracing::TracingRuntime> lola_tracing_runtime{nullptr};
        if (configuration.GetTracingConfiguration().IsTracingEnabled() && tracing_filter_config.has_value())
        {
            const auto number_of_needed_tracing_slots = tracing_filter_config->GetNumberOfTracingSlots(configuration);
            lola_tracing_runtime =
                std::make_unique<lola::tracing::TracingRuntime>(number_of_needed_tracing_slots, configuration);
        }

        auto lola_runtime = std::make_unique<score::mw::com::impl::lola::Runtime>(
            configuration, long_running_threads, std::move(lola_tracing_runtime));
        const auto pair = result.emplace(BindingType::kLoLa, std::move(lola_runtime));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(pair.second, "Failed to emplace lola runtime binding");
    }
    return result;
}
