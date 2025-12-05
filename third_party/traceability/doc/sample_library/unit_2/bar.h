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

#ifndef BAR_H
#define BAR_H

#include "third_party/traceability/doc/sample_library/unit_1/foo.h"

#include <cstdint>
#include <memory>

namespace unit_2
{

// trace: SampleLibrary.SampleRequirement
class Bar final
{
  public:
    explicit Bar(std::unique_ptr<unit_1::Foo>);
    bool AssertNumber() const;

  private:
    std::unique_ptr<unit_1::Foo> foo_;
};

}  // namespace unit_2

#endif  // BAR_H
