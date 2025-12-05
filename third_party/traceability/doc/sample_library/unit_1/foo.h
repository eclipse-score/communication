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

#ifndef FOO_H
#define FOO_H

#include <cstdint>

namespace unit_1
{

class Foo final
{
  public:
    std::uint8_t GetNumber() const;
    void SetNumber(std::uint8_t value);
};

}  // namespace unit_1

#endif  // FOO_H
