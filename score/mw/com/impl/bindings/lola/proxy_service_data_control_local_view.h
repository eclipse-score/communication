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
#ifndef SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_PROXY_SERVICE_DATA_CONTROL_LOCAL_VIEW_H
#define SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_PROXY_SERVICE_DATA_CONTROL_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy_event_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"

#include <score/assert.hpp>

#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

/// ProxyServiceDataControlLocalView is a view class which stores views to the individual objects in shared memory.
///
/// This class will be created by a Proxy on construction. It provides a local view into all the elements within
/// ServiceDataControl. Since the memory layout of ServiceDataControl will never change after construction (e.g. the
/// event_controls_ map will never be resized after construction), each Proxy can simply create a
/// ProxyServiceDataControlLocalView to avoid accessing the shared memory via OffsetPtr. Additional information about
/// the performance impacts of OffsetPtr are described in EventDataControl.
class ProxyServiceDataControlLocalView
{
  public:
    explicit ProxyServiceDataControlLocalView(ServiceDataControl& service_data_control)
        : event_controls_{}, application_id_pid_mapping_{service_data_control.application_id_pid_mapping_}
    {
        for (auto& element : service_data_control.event_controls_)
        {
            const auto insertion_result = event_controls_.emplace(
                std::piecewise_construct, std::forward_as_tuple(element.first), std::forward_as_tuple(element.second));

            const bool was_inserted = insertion_result.second;
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(was_inserted);
        }
    }

    ~ProxyServiceDataControlLocalView() noexcept = default;

    ProxyServiceDataControlLocalView(const ProxyServiceDataControlLocalView&) = delete;
    ProxyServiceDataControlLocalView& operator=(const ProxyServiceDataControlLocalView&) = delete;
    ProxyServiceDataControlLocalView(ProxyServiceDataControlLocalView&&) noexcept = delete;
    ProxyServiceDataControlLocalView& operator=(ProxyServiceDataControlLocalView&& other) noexcept = delete;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::memory::shared::Map<ElementFqId, ProxyEventControlLocalView> event_controls_;

    /// \brief mapping of a proxy's application identifier to its process ID (pid).
    /// \details Every proxy instance for this service shall register itself in this mapping. The identifier used is
    ///          either the 'applicationID' from the global configuration or, if not provided, the process's user ID
    ///          (uid) as a fallback. This mapping is used by proxy instances to detect if they have crashed. Upon
    ///          restart, they would find their application identifier already registered with a different (old) PID.
    ///          Note: In the special case where a consumer application has multiple proxy instances for the very same
    ///          service, they would all use the same application identifier and overwrite the registration with the
    ///          same pid, which is acceptable.
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ApplicationIdPidMapping<score::memory::shared::PolymorphicOffsetPtrAllocator<ApplicationIdPidMappingEntry>>&
        application_id_pid_mapping_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_PROXY_SERVICE_DATA_CONTROL_LOCAL_VIEW_H
