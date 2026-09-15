# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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
    "component",
    "dependable_element",
)

component(
    name = "shared_memory_management",
    components = [
        "//score/memory/shared:managed_memory_resource_unit",
        "//score/memory/shared:shared_memory_factory_unit",
        "//score/memory/shared:shared_memory_resource_unit",
        "//score/memory/shared:memory_resource_registry_unit",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "offset_pointer",
    components = [
        "//score/memory/shared:offset_ptr_unit",
        "//score/memory/shared:polymorphic_offset_ptr_allocator_unit",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "shared_memory_containers",
    components = [
        "//score/memory/shared:containers_unit",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "shared_memory_synchronization",
    components = [
        "//score/memory/shared:flock_mutex_unit",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "typed_and_sealed_memory",
    components = [
        "//score/memory/shared:sealed_shared_memory_unit",
        "//score/memory/shared:typed_memory_unit",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "component_memory",
    components = [
        ":offset_pointer",
        ":shared_memory_containers",
        ":shared_memory_management",
        ":shared_memory_synchronization",
        ":typed_and_sealed_memory",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

dependable_element(
    name = "dependable_element_memory",
    architectural_design = ["//score/memory/dependability/software_architectural_design:memory_architectural_design"],
    assumptions_of_use = [],
    components = [":component_memory"],
    # No FMEA/FMEDA/FTA/DFA has been authored yet for score/memory; this is a
    # documented follow-up once the dependability analysis is available.
    dependability_analysis = [],
    integrity_level = "B",
    maturity = "development",
    requirements = [
        "//score/memory/dependability/assumed_system:assumed_system_requirements",
        "//score/memory/dependability/requirements:feature_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/score_baselibs:futurecpp",
        "//third_party/score_baselibs:os",
    ],
)
