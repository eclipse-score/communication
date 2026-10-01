/*********************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_TEST_DOUBLES_TEST_DOUBLES_H
#define SCORE_MW_SERVICE_TEST_DOUBLES_TEST_DOUBLES_H

#include <cstddef>

namespace score::mw::service::test
{

class FakeProxyBase
{
  public:
    using WhatType = std::size_t;

    virtual WhatType What() const = 0;

    virtual ~FakeProxyBase() = default;
};

class FakeProxy : public FakeProxyBase
{
  public:
    explicit FakeProxy(std::size_t the_question) : the_truth_{the_question} {}

    WhatType What() const override
    {
        return the_truth_;
    }

  private:
    WhatType the_truth_{};
};

class OtherFakeProxyBase
{
  public:
    using WhatType = std::size_t;

    virtual WhatType What() const = 0;

    virtual ~OtherFakeProxyBase() = default;
};

class OtherFakeProxy : public OtherFakeProxyBase
{
  public:
    explicit OtherFakeProxy() : the_truth_{123} {}

    WhatType What() const override
    {
        return the_truth_;
    }

  private:
    const WhatType the_truth_{};
};

}  // namespace score::mw::service::test

#endif  // SCORE_MW_SERVICE_TEST_DOUBLES_TEST_DOUBLES_H
