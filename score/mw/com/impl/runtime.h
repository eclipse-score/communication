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
#ifndef SCORE_MW_COM_IMPL_RUNTIME_H
#define SCORE_MW_COM_IMPL_RUNTIME_H

#include "score/concurrency/long_running_threads_container.h"
#include "score/memory/string_literal.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/i_runtime.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_discovery.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"
#include "score/mw/com/impl/tracing/tracing_runtime.h"
#include "score/mw/com/runtime_configuration.h"

#include <score/optional.hpp>
#include <score/span.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl
{

/**
 * \brief Runtime/entry point to our ara::com implementation.
 *
 * This is a singleton class, which cares for initialization of the entire ara::com subsystem.
 * Initialization is done based on configuration files (manifests), which have been handed over via cmdline to
 * the application.
 *
 * AUTOSAR AP currently does <b>not</b> demand the existence of a Runtime class or singleton, but already hints on it
 * being a maybe future extension. (SWS Communication Management 20-11 chapter 7.1.2 Design decisions).
 *
 * It only demands a static method/func within namespace score::mw::com::runtime
 *
 * \details The singleton implementation is based on a Meyers singleton, which gets returned by getInstanceInternal().
 *          This singleton instance needs to get initialized with a Configuration object.
 *          The various overloads of static Initialize() functions differ regarding where/how the configuration is
 *          loaded/provided. All those Initialize() overloads effectively set the static member initialization_config_,
 *          which then gets finally moved into the Meyers singleton instance in the call to getInstanceInternal().
 *          The public getInstance() Method has two functionalities:
 *          - decide, whether the real Runtime singleton (our Meyers singleton) shall be returned or a mock, if one was
 *            injected.
 *          - if it decides to return the real Runtime singleton via getInstanceInternal(), it checks first if already a
 *            valid configuration has been set/loaded via one of the Initialize() overloads and if not, tries to load
 *            if from default path, before it delegates to getInstanceInternal().
 */
class Runtime final : public IRuntime
{
  public:
    /// \brief static initializer for the runtime. Must be called once per process, which intends to use ara::com
    ///        functionality.
    /// \attention Multiple calls to one of the Initialize() overloads shall be avoided. They may have no effect after
    ///            once our Runtime singleton has been created via getInstance()/getInstanceInternal()
    /// \param runtime_configuration object containing configuration needed to initialize the Runtime
    static void Initialize(const runtime::RuntimeConfiguration& runtime_configuration);

    /// \brief get singleton.
    /// \details Might return either reference to a real Runtime instance or to a mock.
    /// \return singleton ref.
    static IRuntime& getInstance() noexcept;

    /// \brief Inject a mock instance as the runtime singleton. Injecting a nullptr will withdraw the mock again.
    /// \details If a mock instance is injected, a call to getInstance() will just return the mock and no implicit call
    ///          to Initialize() will be done. I.e. any config parsing will be completely bypassed.
    static void InjectMock(IRuntime* const mock) noexcept;

    /// \brief ctor for singleton instance. Should only be used internally.
    /// \details we make this ctor public (although it somehow breaks the singleton pattern) since this
    ///          impl::Runtime isn't user facing and just internally used. Having a public ctor eases life
    ///          in so many places!
    /// \param config configuration, which was build up during Runtime::Initialize().
    explicit Runtime(std::pair<Configuration&&, score::cpp::optional<tracing::TracingFilterConfig>&&> configs);

    /// \brief Runtime is not copyable/copy-assignable since it shall have singleton semantic.
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) & = delete;
    Runtime(Runtime&&) noexcept = delete;
    Runtime& operator=(Runtime&&) & noexcept = delete;

    ~Runtime() noexcept override;

    /// \brief Implements score::mw::com::runtime::ResolveInstanceIds
    /// \param specifier specifier for which resolution shall be done.
    /// \return resolved instance identifiers for given _specifier_
    // Suppress "AUTOSAR C++14 A10-3-1" rule finding: Virtual function declaration shall contain exactly one of the
    // three specifiers: (1) virtual, (2) override, (3) final. Gcc compiler bug leads to a compiler warning if
    // override is not added, even if final keyword is there. (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78010)
    // coverity[autosar_cpp14_a10_3_1_violation]
    std::vector<InstanceIdentifier> resolve(const InstanceSpecifier& specifier) const override final;

    /// \brief see IRuntime::GetBindingRuntime
    // coverity[autosar_cpp14_a10_3_1_violation]
    IRuntimeBinding* GetBindingRuntime(const BindingType binding) const noexcept override final;

    // coverity[autosar_cpp14_a10_3_1_violation]
    IServiceDiscovery& GetServiceDiscovery() noexcept override final;

    /// \brief see IRuntime::GetTracingFilterConfig
    // coverity[autosar_cpp14_a10_3_1_violation]
    const tracing::ITracingFilterConfig* GetTracingFilterConfig() const noexcept override final;

    /// \brief see IRuntime::GetTracingRuntime
    // coverity[autosar_cpp14_a10_3_1_violation]
    tracing::ITracingRuntime* GetTracingRuntime() const noexcept override final;

  private:
    /// \return static Runtime (the real one - not a mock!) configured based on the configuration set by one of the
    ///         static Initialize() overloads.
    /// \pre the internal static initialization_config_ has to be initialized with a Configuration.
    static Runtime& getInstanceInternal() noexcept;

    /// \brief Helper function to store the configuration in the initialization_config_ and sets the configuration in
    /// InstanceIdentifier
    static void StoreConfiguration(Configuration config) noexcept;

    /// \brief pointer to a mock to be used (set via InjectMock())
    static score::mw::com::impl::IRuntime* mock_;

    /// \brief mutex to synchronize potentially concurrent calls to initialization logic of Runtime.
    static std::mutex mutex_;

    /// \brief flag, whether the runtime is now locked in the sense, that the singleton has been already created
    ///        with a given configuration.
    static bool runtime_initialization_locked_;

    /// \brief static configuration set by one of the static Initialize() overloads. Will then finally get moved into
    ///        the singleton instance member configuration_.
    static score::cpp::optional<Configuration> initialization_config_;

    /// \brief configuration
    Configuration configuration_;

    /// \brief Tracing configuration parsed from json
    ///
    /// Will be filled only if tracing is enabled in the configuration_ and the tracing json file can be found and
    /// successfully parsed.
    score::cpp::optional<tracing::TracingFilterConfig> tracing_filter_configuration_;

    /// \brief binding specific runtimes (runtime extensions).
    std::unordered_map<BindingType, std::unique_ptr<IRuntimeBinding>> runtime_bindings_;

    /// \brief Tracing Runtime which encapsulates all calls to GenericTraceLibrary.
    ///        This pointer will only be set to a value when a tracing_filter_configuration_ is set.
    std::unique_ptr<tracing::ITracingRuntime> tracing_runtime_;

    /// \brief Service Discovery
    ServiceDiscovery service_discovery_;

    /// \brief Executor for long-running tasks, that is handed down to binding-specific runtimes to be also used in
    ///        their context
    ///        Should stay last attribute so that it is destructed first
    concurrency::LongRunningThreadsContainer long_running_threads_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_RUNTIME_H
