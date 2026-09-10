/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/someip/deployment_resources.h"
#include "score/mw/com/impl/bindings/someip/proxy.h"
#include "score/mw/com/impl/bindings/someip/proxy_event.h"
#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"

#include <memory>
#include <string_view>
namespace score::mw::com::impl::someip
{

/// \brief Creates the SOME/IP service element bindings for a given parent binding.
///
/// The parent is passed as the already downcast binding specific type. The binding independent event binding factories
/// are responsible for that downcast, since only they know that the parent binding and the service element binding
/// have to belong to the same technical binding.
template <typename SampleType>
struct EventBindingFactory final
{
    static std::unique_ptr<ProxyEvent<SampleType>> CreateProxyEvent(Proxy& parent,
                                                                    const SomeIpEventId event_id,
                                                                    const std::string_view event_name) noexcept
    {
        return std::make_unique<ProxyEvent<SampleType>>(parent, event_id, event_name);
    }

    static std::unique_ptr<SkeletonEvent<SampleType>> CreateSkeletonEvent(Skeleton& parent,
                                                                          const SomeIpEventId event_id,
                                                                          const std::string_view event_name) noexcept
    {
        return std::make_unique<SkeletonEvent<SampleType>>(parent, event_id, event_name);
    }
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_BINDING_FACTORY_H
