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
#include "score/mw/com/test/methods/non_trivial_constructors/test_method_datatype.h"

namespace score::mw::com::test
{

NonTriviallyConstructibleType operator+(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b)
{
    return NonTriviallyConstructibleType{a.value_ + b.value_};
}

bool operator==(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b)
{
    return a.value_ == b.value_;
}

bool operator!=(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b)
{
    return !(a == b);
}

std::ostream& operator<<(std::ostream& os, const NonTriviallyConstructibleType& obj)
{
    os << obj.value_;
    return os;
}

}  // namespace score::mw::com::test
