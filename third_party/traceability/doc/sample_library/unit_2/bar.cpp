/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "third_party/traceability/doc/sample_library/unit_2/bar.h"

#include <cassert>

namespace
{
std::uint8_t kExpectedNumber = 42u;
}

namespace unit_2
{

Bar::Bar(std::unique_ptr<unit_1::Foo> foo) : foo_{std::move(foo)} {}

bool Bar::AssertNumber() const
{
    assert(foo_ != nullptr);
    return foo_->GetNumber() == kExpectedNumber;
}
}  // namespace unit_2
