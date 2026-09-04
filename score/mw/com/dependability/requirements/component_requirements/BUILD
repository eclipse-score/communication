# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component_requirements",
)
load(
    "@score_tooling//validation/ai_checker:ai_checker.bzl",
    "trlc_requirements_ai_test",
)

component_requirements(
    name = "component_requirements_ipc",
    srcs = [
        "GenericSkeletonAndServiceElements/component_requirements_ipc_generic_skeleton.trlc",
        "GenericSkeletonAndServiceElements/component_requirements_ipc_generic_skeleton_event.trlc",
        "GenericSkeletonAndServiceElements/component_requirements_ipc_generic_skeleton_field.trlc",
        "SkeletonAndServiceElements/component_requirements_ipc_skeleton.trlc",
        "SkeletonAndServiceElements/component_requirements_ipc_skeleton_event.trlc",
        "SkeletonAndServiceElements/component_requirements_ipc_skeleton_field.trlc",
        "binding_independent/proxy_and_service_elements/component_requirements_ipc_proxy.trlc",
        "binding_independent/proxy_and_service_elements/component_requirements_ipc_proxy_event.trlc",
        "binding_independent/proxy_and_service_elements/component_requirements_ipc_proxy_field.trlc",
        "component_requirements_ipc.trlc",
        "component_requirements_ipc_generic_proxy.trlc",
        "component_requirements_ipc_methods.trlc",
        "component_requirements_ipc_partial_restart.trlc",
        "component_requirements_ipc_tracing.trlc",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
    deps = [
        "//score/mw/com/dependability/assumed_system:assumed_system_requirements",
        "//score/mw/com/dependability/requirements/feature_requirements:feature_requirements_ipc",
    ],
)

trlc_requirements_ai_test(
    name = "requirements_ai_check",
    batch_size = 10,
    reqs = [":component_requirements_ipc"],
    tags = ["manual"],
)

component_requirements(
    name = "component_requirements_e2e",
    srcs = [
        "component_requirements_e2e_api.trlc",
        "component_requirements_e2e_configuration.trlc",
        "component_requirements_e2e_lifecycle.trlc",
        "component_requirements_e2e_methods.trlc",
        "component_requirements_e2e_proxy.trlc",
        "component_requirements_e2e_skeleton.trlc",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
    deps = [
        "//score/mw/com/dependability/assumed_system:assumed_system_requirements",
        "//score/mw/com/dependability/requirements/feature_requirements:feature_requirements_e2e",
    ],
)

trlc_requirements_ai_test(
    name = "requirements_ai_check_e2e",
    batch_size = 10,
    reqs = [":component_requirements_e2e"],
    tags = ["manual"],
)

component_requirements(
    name = "component_requirements_e2e_error_handling",
    srcs = ["component_requirements_e2e_error_handling.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = [
        "//score/mw/com/dependability:__subpackages__",
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/dependability/assumed_system:assumed_system_requirements",
        "//score/mw/com/dependability/requirements/feature_requirements:feature_requirements_e2e_error_handling",
    ],
)

trlc_requirements_ai_test(
    name = "requirements_ai_check_e2e_error_handling",
    batch_size = 10,
    reqs = [":component_requirements_e2e_error_handling"],
    tags = ["manual"],
)
