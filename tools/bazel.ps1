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
#
# Bazelisk hook (https://github.com/bazelbuild/bazelisk#toolsbazel) for
# Windows hosts. Bazelisk looks for `tools/bazel.ps1` regardless of
# architecture on Windows.
#
# Windows is not an officially supported development platform for this
# project (see CONTRIBUTING.md); this script only warns about that and never
# blocks the actual bazel invocation, forwarding straight to $Env:BAZEL_REAL.

Write-Warning "Windows is not an officially supported development platform for this project."
Write-Warning "Before filing any bug report, please try reproducing it on a supported platform."

& $Env:BAZEL_REAL @Args
exit $LASTEXITCODE
