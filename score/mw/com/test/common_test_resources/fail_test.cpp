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

#include "score/mw/com/test/common_test_resources/fail_test.h"

#include <iostream>

namespace score::mw::com::test::detail
{

void fail_test_(std::stringstream&& strstr)
{
    strstr << "\033[0m  \033[1m\033[41mTEST FAILED\033[0m\n";
    std::cerr << std::move(strstr).str() << std::flush;
    std::_Exit(EXIT_FAILURE);
}

}  // namespace score::mw::com::test::detail
