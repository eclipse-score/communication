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
#include "score/mw/com/impl/runtime.h"

#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/plumbing/binding_runtime_factory.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config_parser.h"
#include "score/mw/com/impl/tracing/i_binding_tracing_runtime.h"

#include "score/memory/shared/memory_resource_registry.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"
#include "score/mw/log/runtime.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <string_view>
#include <utility>

namespace
{

inline void warn_double_init()
{
    score::mw::log::LogWarn("lola") << "score::mw::com::impl::Runtime is already initialized! Redundant call to a "
                                       "Runtime::Initialize() overload within production code needs to be checked.";
}

inline void error_double_init()
{
    score::mw::log::LogError("lola")
        << "score::mw::com::impl::Runtime is already initialized and locked! Redundant call to a "
           "Runtime::Initialize() overload without effect within production code needs to be checked.";
}

/// \brief Forces initialization of all static dependencies our static Runtime has.
/// \details To avoid a static destruction order fiasco, where we access objects, which are located in other static
///          contexts,  from our Runtime static context, when those other static contexts have already been destroyed,
///          we "touch" those other static contexts (make sure that they get initialized) BEFORE our own static Runtime
///          context gets initialized. This way, we make sure, that those other static contexts, where we depend on, are
///          outliving our static context and we are always save to access it.
///          Currently we see two "static dependencies" we do have:
///          - logging: mw::log has some static context and we use this logging facility everywhere in our mw::com code.
///          - MemoryResourceRegistry of lib/memory/shared: This is also a static singleton and all our
///          proxies/skeletons
///            depend on it, as e.g. in their destructors we are unregistering memory-resources from
///            MemoryResourceRegistry. mw::com/LoLa supports/allows, that proxy/skeleton instances are residing in the
///            static context of our impl::Runtime! (We do only forbid via AoU, that users put proxies/skeletons in
///            their own static context).
///            Creating proxies/skeletons in our static Runtime context might implicitly happen (is allowed), when a
///            user creates e.g. a proxy within a FidService-callback. This callback is then handed over to the
///            StartFindService-API and stored in our service discovery, which is part of our static Runtime context!
///            So this callback will be executed in our static Runtime context and we have to make sure, that
///            MemoryResourceRegistry is available.
void TouchStaticDependencies()
{
    score::cpp::ignore = score::mw::log::detail::Runtime::GetRecorder();
    score::cpp::ignore = score::mw::log::detail::Runtime::GetFallbackRecorder();
    score::cpp::ignore = score::memory::shared::MemoryResourceRegistry::getInstance();
}

}  // namespace

namespace score::mw::com::impl
{

using TracingFilterConfig = tracing::TracingFilterConfig;

IRuntime* Runtime::mock_ = nullptr;
score::cpp::optional<Configuration> Runtime::initialization_config_{};
bool Runtime::runtime_initialization_locked_{false};

std::mutex score::mw::com::impl::Runtime::mutex_{};

namespace
{

score::cpp::optional<TracingFilterConfig> ParseTraceConfig(const Configuration& configuration)
{
    if (!configuration.GetTracingConfiguration().IsTracingEnabled())
    {
        return {};
    }
    const auto trace_filter_config_path = configuration.GetTracingConfiguration().GetTracingFilterConfigPath();
    score::Result<TracingFilterConfig> tracing_config_result = tracing::Parse(trace_filter_config_path, configuration);
    if (!tracing_config_result.has_value())
    {
        score::mw::log::LogError("lola") << "Parsing tracing config failed with error: "
                                         << tracing_config_result.error();
        return {};
    }
    return std::move(tracing_config_result).value();
}

}  // namespace

void Runtime::Initialize(const runtime::RuntimeConfiguration& runtime_configuration)
{
    std::lock_guard<std::mutex> lock{mutex_};
    if (runtime_initialization_locked_)
    {
        error_double_init();
        return;
    }

    if (initialization_config_.has_value())
    {
        warn_double_init();
    }

    auto config = configuration::Parse(runtime_configuration.GetConfigurationPath().Native());
    StoreConfiguration(std::move(config));
}

auto Runtime::getInstance() noexcept -> IRuntime&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }
    else
    {
        return getInstanceInternal();
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'initialization_config_.value()'
// in case initialization_config_ doesn't have a value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
Runtime& Runtime::getInstanceInternal() noexcept
{
    TouchStaticDependencies();
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.". This cannot be constexpr as the lambda function executes at runtime.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static Runtime instance{([]() -> std::pair<Configuration, score::cpp::optional<TracingFilterConfig>> {
        std::lock_guard<std::mutex> lock{mutex_};
        runtime_initialization_locked_ = true;
        if (!initialization_config_.has_value())
        {
            runtime::RuntimeConfiguration runtime_configuration{};
            auto configuration = configuration::Parse(runtime_configuration.GetConfigurationPath().Native());
            auto tracing_config = ParseTraceConfig(configuration);
            return std::make_pair(std::move(configuration), std::move(tracing_config));
        }
        else
        {
            auto tracing_config = ParseTraceConfig(initialization_config_.value());
            auto configuration_pair =
                std::make_pair(std::move(initialization_config_.value()), std::move(tracing_config));
            initialization_config_.reset();
            return configuration_pair;
        }
    })()};
    return instance;
}

Runtime::Runtime(std::pair<Configuration&&, score::cpp::optional<TracingFilterConfig>&&> configs)
    : IRuntime{},
      configuration_{std::move(std::get<0>(configs))},
      tracing_filter_configuration_{std::move(std::get<1>(configs))},
      tracing_runtime_{nullptr},
      service_discovery_{*this},
      long_running_threads_{}
{
    binding_runtimes_ = BindingRuntimeFactory::CreateBindingRuntimes(
        configuration_, long_running_threads_, tracing_filter_configuration_);
    if (configuration_.GetTracingConfiguration().IsTracingEnabled())
    {
        if (tracing_filter_configuration_.has_value())
        {
            // LCOV_EXCL_START (Tool incorrectly marks this line as not covered. However, the lines before and after
            // are covered so this is clearly an error by the tool. Suppression can be removed when bug is fixed in
            // Ticket-Ticket-184255).
            std::unordered_map<BindingType, tracing::IBindingTracingRuntime*> binding_tracing_runtimes{};
            // LCOV_EXCL_STOP
            for (auto& binding_runtime : binding_runtimes_)
            {
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
                    binding_runtime.second->GetTracingRuntime() != nullptr,
                    "Binding specific runtime has no tracing runtime although tracing is enabled!");
                score::cpp::ignore = binding_tracing_runtimes.emplace(binding_runtime.first,
                                                                      binding_runtime.second->GetTracingRuntime());
            }
            tracing_runtime_ = std::make_unique<tracing::TracingRuntime>(std::move(binding_tracing_runtimes));
        }
    }
}

Runtime::~Runtime() noexcept
{
    mw::log::LogDebug("lola") << "Starting destrcution of mw::com runtime";
}

std::vector<InstanceIdentifier> Runtime::resolve(const InstanceSpecifier& specifier) const
{
    std::vector<InstanceIdentifier> result;
    const auto instanceSearch = configuration_.GetServiceInstances().find(specifier);
    if (instanceSearch != configuration_.GetServiceInstances().end())
    {
        // @todo: Right now we don't support multi-binding, if we do, we need to have some kind of loop
        const auto type_deployment = configuration_.GetServiceTypes().find(instanceSearch->second.service_);
        // LCOV_EXCL_BR_START defensive programming: The configuration parser ensures that if a matching service
        // instance is available, there is also a matching service type available. Because parsing of the configuration
        // is automatically done before instantiating the runtime, this condition is always positive. To increase the
        // robustness of the code, we still check for this condition.

        // TM: Will we remove such extra robustness check?
        if (type_deployment != configuration_.GetServiceTypes().cend())
        {
            result.push_back(make_InstanceIdentifier(instanceSearch->second, type_deployment->second));
        }
        else
        {
            // LCOV_EXCL_START see comment before if-statement
            mw::log::LogError("lola") << "Did not find a matching service type for the specifier" << specifier;
            // LCOV_EXCL_STOP
        }
        // LCOV_EXCL_BR_STOP
    }

    return result;
}

auto Runtime::GetBindingRuntime(const BindingType binding) const noexcept -> IBindingRuntime*
{
    auto search = binding_runtimes_.find(binding);
    if (search != binding_runtimes_.cend())
    {
        return search->second.get();
    }
    return nullptr;
}

auto Runtime::GetServiceDiscovery() noexcept -> IServiceDiscovery&
{
    // The signature of this function is part of the detailled design.
    // coverity[autosar_cpp14_a9_3_1_violation]
    return service_discovery_;
}

auto Runtime::GetTracingFilterConfig() const noexcept -> const tracing::ITracingFilterConfig*
{
    if (!tracing_filter_configuration_.has_value())
    {
        return nullptr;
    }
    return &tracing_filter_configuration_.value();
}

tracing::ITracingRuntime* Runtime::GetTracingRuntime() const noexcept
{
    return tracing_runtime_.get();
}

void Runtime::InjectMock(IRuntime* const mock) noexcept
{
    mock_ = mock;
}

void Runtime::StoreConfiguration(Configuration config) noexcept
{
    score::cpp::ignore = initialization_config_.emplace(std::move(config));
    InstanceIdentifier::SetConfiguration(&(*initialization_config_));
}

}  // namespace score::mw::com::impl
