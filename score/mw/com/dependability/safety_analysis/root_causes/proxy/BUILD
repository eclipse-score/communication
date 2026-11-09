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
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/call_method:call_method_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/construction:proxy_cannot_be_created_with_handle_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/find_service:service_is_found_but_is_not_currently_offered_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/find_service:service_not_found_but_is_currently_offered_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/find_service:wrong_service_found_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/generic_proxy/generic_proxy_event/getdatatypedizeinfo_from_generic_proxy_event:the_size_returned_is_bigger_then_the_actual_value_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/generic_proxy/generic_proxy_event/has_serialized_format:wrong_indication_if_format_is_serialized_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/generic_proxy/getting_access_to_generic_proxy_events:map_containing_nonexistent_events_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_free_sample_count_for_an_event:returns_wrong_free_sample_count_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:callback_not_called_with_samples_received_from_skeleton_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:data_pointed_to_by_sample_ptr_has_different_value_to_sample_sent_by_skeleton_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:receiver_is_called_more_times_than_allowed_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:returned_call_count_different_to_number_of_times_receiver_called_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:returns_an_error_although_called_receiver_correct_number_of_times_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:returns_not_subscribed_error_although_in_active_subscription_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:returns_success_although_did_not_call_receiver_correct_number_of_times_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:returns_success_although_not_in_active_subscription_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/get_new_samples_on_an_event:sample_ptr_returned_which_was_received_before_last_get_new_samples_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/set_receive_handler_for_an_event:set_receive_handler_for_an_event_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/start_find_service_construction:start_find_service_callback_is_redundantly_called_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/stop_find_service:wrong_startfindservice_is_stopped_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/subscribe_for_an_event:subscribe_for_an_event_fta",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy/unsubscribe_for_an_event:unsubscribe_for_an_event_fta",
    ],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)
