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
#include "score/mw/com/impl/plumbing/proxy_method_binding_factory_impl.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

namespace score::mw::com::impl
{

LolaMethodInstanceDeployment::QueueSize GetQueueSize(HandleType parent_handle, const std::string& method_name_str)
{
    const auto& lola_service_instance_deployment = GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(
        parent_handle.GetServiceInstanceDeployment());
    auto method_it = lola_service_instance_deployment.methods_.find(method_name_str);
    auto contains_method = method_it != lola_service_instance_deployment.methods_.end();
    if (!contains_method)
    {
        mw::log::LogFatal("lola") << "Provided a method name which can not be found in LolaServiceInstanceDeployment";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Provided a method name which can not be found in LolaServiceInstanceDeployment");
    }
    const auto& lola_method_instance_deployment = method_it->second;

    auto queue_size_has_no_value = !lola_method_instance_deployment.queue_size_.has_value();
    if (queue_size_has_no_value)
    {
        mw::log::LogFatal("lola")
            << "ProxyMethod can not be created if queue_size is not configured on the proxy side.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false,
                               "ProxyMethod can not be created if queue_size is not configured on the proxy side.");
    }
    return lola_method_instance_deployment.queue_size_.value();
}
}  // namespace score::mw::com::impl
