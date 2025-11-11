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

"""
Source Code Linker package for traceability extraction.

This package provides functionality for extracting traceability links from source code
and generating lobster-compatible output files.
"""

from .main import SourceCodeLinker

__version__ = "1.0.0"
__all__ = ["SourceCodeLinker"]
