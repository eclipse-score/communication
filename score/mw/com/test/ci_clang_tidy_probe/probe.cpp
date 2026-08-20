/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/test/ci_clang_tidy_probe/probe.h"

namespace score::mw::com::test::ci_clang_tidy_probe
{

int AddMagicOffset(int value)
{
    // Intentional clang-tidy violation (readability-magic-numbers): 4217 is an
    // unexplained literal used directly in an expression.
    return value + 4217;
}

}  // namespace score::mw::com::test::ci_clang_tidy_probe
