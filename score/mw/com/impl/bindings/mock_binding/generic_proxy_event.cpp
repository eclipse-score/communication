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
#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"

namespace score::mw::com::impl::mock_binding
{

Result<std::size_t> GenericProxyEvent::GetNewFakeSamples(typename GenericProxyEventBinding::Callback&& callable,
                                                         TrackerGuardFactory& tracker)
{
    const std::size_t num_samples = std::min(fake_samples_.size(), tracker.GetNumAvailableGuards());
    const auto start_iter = fake_samples_.end() - static_cast<typename FakeSamples::difference_type>(num_samples);
    auto sample = std::make_move_iterator(start_iter);

    while (sample.base() != fake_samples_.end())
    {
        std::optional<SampleReferenceGuard> guard = tracker.TakeGuard();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(guard.has_value(), "No guards available.");

        SamplePtr<void> ptr = *sample;
        impl::SamplePtr<void> impl_ptr = this->MakeSamplePtr(std::move(ptr), std::move(*guard));

        const tracing::ITracingRuntime::TracePointDataId dummy_trace_point_data_id{0U};
        callable(std::move(impl_ptr), dummy_trace_point_data_id);
        ++sample;
    }

    fake_samples_.clear();
    return {num_samples};
}

}  // namespace score::mw::com::impl::mock_binding
