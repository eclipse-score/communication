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

#include "score/mw/com/impl_new/binding_independent/skeleton/skeleton/skeleton.h"

#include <gtest/gtest.h>

TEST(Skeleton, PrepareOffer)
{
    ::testing::Test::RecordProperty("lobster-tracing", "SkeletonComponent.SkeletonWorks");

    ::testing::Test::RecordProperty("given", "a default-constructed Skeleton instance");
    score::mw::com::impl::Skeleton unit{};

    ::testing::Test::RecordProperty("when", "PrepareOffer is called");
    ::testing::Test::RecordProperty("then", "it does not throw");
    EXPECT_NO_THROW(unit.PrepareOffer());
}

TEST(Skeleton, PrepareOfferUnit)
{
    ::testing::Test::RecordProperty("lobster-tracing", "SkeletonUnit.SkeletonOfferServiceDoesNotThrow");

    ::testing::Test::RecordProperty("given", "a default-constructed Skeleton instance");
    score::mw::com::impl::Skeleton unit{};

    ::testing::Test::RecordProperty("when", "PrepareOffer is called");
    ::testing::Test::RecordProperty("then", "it does not throw");
    EXPECT_NO_THROW(unit.PrepareOffer());
}
