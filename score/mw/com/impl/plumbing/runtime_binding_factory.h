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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_RUNTIME_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_RUNTIME_BINDING_FACTORY_H

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/i_runtime_binding.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"

#include <score/optional.hpp>

#include "score/concurrency/executor.h"

#include <map>
#include <memory>
#include <unordered_map>

namespace score::mw::com::impl
{

class RuntimeBindingFactory final
{
  public:
    static std::unordered_map<BindingType, std::unique_ptr<IRuntimeBinding>> CreateBindingRuntimes(
        Configuration& configuration,
        concurrency::Executor& long_running_threads,
        const score::cpp::optional<tracing::TracingFilterConfig>& tracing_filter_config);
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_RUNTIME_BINDING_FACTORY_H
