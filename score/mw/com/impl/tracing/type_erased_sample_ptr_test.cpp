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
#include "score/mw/com/impl/tracing/type_erased_sample_ptr.h"

#include <gtest/gtest.h>
#include <utility>

namespace score::mw::com::impl::tracing
{

class DestructorTracer
{
  public:
    DestructorTracer(bool& was_destructed) : is_owner_{true}, was_destructed_{was_destructed} {}
    ~DestructorTracer()
    {
        if (is_owner_)
        {
            was_destructed_ = true;
        }
    }

    DestructorTracer(const DestructorTracer&) = delete;
    DestructorTracer& operator=(const DestructorTracer&) = delete;
    DestructorTracer& operator=(DestructorTracer&&) = delete;

    DestructorTracer(DestructorTracer&& other) : is_owner_{true}, was_destructed_{other.was_destructed_}
    {
        other.is_owner_ = false;
    }

  private:
    bool is_owner_;
    bool& was_destructed_;
};

TEST(TypeErasedSamplePtrTest, ObjectPassedToConstructorIsDestroyedWhenTypeErasedPtrIsDestroyed)
{
    bool was_destructed{false};
    DestructorTracer destructor_tracer{was_destructed};
    {
        TypeErasedSamplePtr type_erased_ptr{std::move(destructor_tracer)};
        EXPECT_FALSE(was_destructed);
    }
    EXPECT_TRUE(was_destructed);
}

}  // namespace score::mw::com::impl::tracing
