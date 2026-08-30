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
    "assumed_system_requirements",
    "assumptions_of_use",
)

# NOTE: assumptions_of_use() (as of score_tooling's "harmonize aous" rework)
# renders RST only from raw .trlc/.rst files passed directly in `srcs`; labels
# to existing TrlcProviderInfo-emitting targets (e.g. a separate
# trlc_requirements target) are routed to `deps` for cross-reference parsing
# only, and are NOT rendered. So aous.trlc must be passed directly here
# instead of via an intermediate trlc_requirements target (spec defaults to
# score_requirements_model already, matching what that target used to set
# explicitly).
assumptions_of_use(
    name = "aous",
    srcs = ["aous.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
)

assumed_system_requirements(
    name = "assumed_system_requirements",
    srcs = ["assumed_system_requirements.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
)
