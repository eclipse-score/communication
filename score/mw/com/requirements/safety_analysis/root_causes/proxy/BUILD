# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

filegroup(
    name = "root_causes_proxy",
    srcs = [
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/construction:construction_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/find_service:service_not_found_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/find_service:wrong_service_found_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/generic_proxy/generic_proxy_event/getsamplesize_from_generic_proxy_event:the_size_returned_is_bigger_then_the_actual_value_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/generic_proxy/getting_access_to_generic_proxy_events:map_containing_nonexistent_events_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:callback_invoked_with_wrong_data_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:callback_not_invoked_despite_samples_available_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/start_find_service_construction:start_find_service_callback_is_redundantly_called_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/subscribe_for_an_event:subscribe_to_wrong_event_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/subscribe_for_an_event:subscribe_with_wrong_max_sample_count_fta",
        "//score/mw/com/requirements/safety_analysis/root_causes/proxy/unsubscribe_for_an_event:does_not_unsubscribe_fta",
    ],
    visibility = ["//visibility:public"],
)
