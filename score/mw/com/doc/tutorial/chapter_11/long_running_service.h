/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_TUTORIAL_LONG_RUNNING_SERVICE_H
#define SCORE_MW_COM_TUTORIAL_LONG_RUNNING_SERVICE_H

#include "score/mw/com/types.h"

#include <array>
#include <cstdint>
#include <type_traits>

namespace score::mw::com::tutorial
{

// A fixed-capacity character buffer used to transport the *serialized* form of an InstanceIdentifier
// (InstanceIdentifier::ToString()) as a method argument. Method arguments are transported through shared memory and are
// serialized via a plain memcpy, therefore every method argument type must be trivially copyable - which a
// std::array<char, N> is. The capacity is chosen generously so that it can hold the serialized InstanceIdentifier of
// the callback service instance used in this chapter (the consumer asserts at runtime that the serialized form actually
// fits).
using SerializedInstanceIdentifier = std::array<char, 2048>;

static_assert(std::is_trivially_copyable_v<SerializedInstanceIdentifier>,
              "SerializedInstanceIdentifier must be trivially copyable to be usable as a method argument.");

// The "long running" service. Its single method PostLongRunningJob does - semantically - NOT execute the long running
// job synchronously. Instead it just *queues* the job at the provider and returns immediately; its bool return value
// only signals whether the job could be queued. The actual result is delivered asynchronously later via a *callback*:
// the provider calls SetJobResult() on a callback service instance the consumer provides (see
// LongRunningServiceResultInterface below).
//
// To let the provider find exactly that one callback instance (out of potentially many consumers, each providing its
// own callback instance), the consumer passes the *serialized InstanceIdentifier* of its callback instance as the third
// argument. This is the "power-user" use case of directly using an InstanceIdentifier that is shown in this chapter.
template <typename Trait>
class LongRunningServiceInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // PostLongRunningJob(job_argument, job_number, serialized_callback_instance_identifier) -> queued_successfully
    //  - job_argument: a dummy value standing in for the actual job input.
    //  - job_number:   chosen by the caller; echoed back in the SetJobResult() callback so the caller can correlate a
    //                  result with the request that produced it.
    //  - serialized_callback_instance_identifier: InstanceIdentifier::ToString() of the consumer's callback instance.
    //  - return:       true if the job was queued successfully, false otherwise.
    typename Trait::template Method<bool(std::uint32_t, std::uint32_t, SerializedInstanceIdentifier)>
        post_long_running_job_{*this, "PostLongRunningJob"};
};

// The callback service. Each consumer of LongRunningService provides its *own* instance of this service. The provider
// calls SetJobResult() on it once the previously queued job has finished.
template <typename Trait>
class LongRunningServiceResultInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // SetJobResult(job_result, job_number) -> void
    //  - job_result: a dummy value standing in for the actual job result.
    //  - job_number: the job_number of the PostLongRunningJob() call this result belongs to.
    typename Trait::template Method<void(std::uint32_t, std::uint32_t)> set_job_result_{*this, "SetJobResult"};
};

using LongRunningServiceProxy = score::mw::com::AsProxy<LongRunningServiceInterface>;
using LongRunningServiceSkeleton = score::mw::com::AsSkeleton<LongRunningServiceInterface>;
using LongRunningServiceResultProxy = score::mw::com::AsProxy<LongRunningServiceResultInterface>;
using LongRunningServiceResultSkeleton = score::mw::com::AsSkeleton<LongRunningServiceResultInterface>;

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_LONG_RUNNING_SERVICE_H
