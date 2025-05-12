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
#ifndef SCORE_MW_COM_IMPL_I_RUNTIME_H
#define SCORE_MW_COM_IMPL_I_RUNTIME_H

#include "score/mw/com/impl/i_runtime_binding.h"
#include "score/mw/com/impl/i_service_discovery.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/tracing/configuration/i_tracing_filter_config.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime.h"

#include <vector>

namespace score::mw::com::impl
{

/// \brief Interface for our generic (binding independent) runtime.
/// \details Interface is introduced for testing/mocking reasons.
class IRuntime
{
  public:
    IRuntime() = default;
    virtual ~IRuntime();

    /**
     * \brief Implements score::mw::com::runtime::ResolveInstanceIds
     */
    virtual std::vector<InstanceIdentifier> resolve(const InstanceSpecifier&) const = 0;

    /// \brief returns binding specific runtime for given _binding_
    /// \param binding binding type
    /// \return  Binding specific runtime (needs to be down-casted) or nullptr in case there is no binding runtime for
    ///          the given type (due to configuration settings)
    virtual IRuntimeBinding* GetBindingRuntime(const BindingType binding) const noexcept = 0;

    virtual IServiceDiscovery& GetServiceDiscovery() noexcept = 0;

    /// \brief returns the tracing related part of the Runtime
    /// \return nullptr, if tracing is not enabled, ptr to TracingRuntime else.
    virtual tracing::ITracingRuntime* GetTracingRuntime() const noexcept = 0;

    /// \brief Returns TracingFilterConfig that is parsed from a json config file.
    /// \return TracingFilterConfig pointer or nullptr in case the config file could not be found or parsed.
    virtual const tracing::ITracingFilterConfig* GetTracingFilterConfig() const noexcept = 0;

  protected:
    IRuntime(const IRuntime&) = default;
    IRuntime& operator=(const IRuntime&) & = default;
    IRuntime(IRuntime&&) noexcept = default;
    IRuntime& operator=(IRuntime&&) & noexcept = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_I_RUNTIME_H
