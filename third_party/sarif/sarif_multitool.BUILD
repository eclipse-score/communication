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

# `Sarif.Multitool` is a self-contained, single-file .NET publish (bundles the
# .NET runtime; no `dotnet`/`npm`/`npx`/network access needed at runtime), so
# it runs standalone. `data = glob(["**"])` keeps its (unused but harmless)
# companion files (*.pdb, *.xml, *.dll.config) alongside it in runfiles, in
# case of any relative-path lookups.
sh_binary(
    name = "sarif_multitool_cli",
    srcs = ["Sarif.Multitool"],
    data = glob(
        ["**"],
        exclude = ["Sarif.Multitool"],
    ),
    visibility = ["//visibility:public"],
)
