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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "architectural_design")

architectural_design(
    name = "software_architectural_design",
    dynamic = [
        "dynamic_design/get_path.puml",
        "dynamic_design/is_shm_in_typed_memory.puml",
        "dynamic_design/remove_memory_resource.puml",
        "dynamic_design/unlink_file_system_entry.puml",
        "dynamic_design/offset_ptr_allocate.puml",
        "dynamic_design/offset_ptr_deallocate.puml",
        "dynamic_design/enable_offset_ptr_bounds_checking.puml",
        "dynamic_design/offset_ptr_get_memory_resource_proxy.puml",
        "dynamic_design/offset_ptr_do_pointer_arithmetic.puml",
        "dynamic_design/offset_ptr_do_pointer_target_access.puml",
        "dynamic_design/offset_ptr_is_valid.puml",
        "dynamic_design/allocator_allocate.puml",
        "dynamic_design/create_shm_resource.puml",
        "dynamic_design/open_shm_resource.puml",
        "dynamic_design/dereference_offset_ptr.puml",
    ],
    internal_api = [
        #"internal_api.puml",
    ],
    maturity = "development",
    # package = "memory_shared_api",
    public_api = [
        #"public_api.puml",  # rejected by the rules_score PlantUML parser
    ],
    static = [
        "static_design/overall.puml",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/memory/shared/dependability:__subpackages__"],
)
