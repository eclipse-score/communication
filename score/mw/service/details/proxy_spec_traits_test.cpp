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

#include "score/mw/service/details/proxy_spec_traits.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

namespace score::mw::service::details
{
namespace
{

TEST(DeriveUserCallbackParameter, FromSingleInstanceHolder)
{
    class MyInstance
    {
    };

    // Given a single instance holder of arbitrary type
    auto holder = SingleInstanceHolderCreator<MyInstance>::Construct<MyInstance>();
    ASSERT_NE(holder, nullptr);

    // When attempting to derive the user callback parameter from it
    // Then no exception is expected
    EXPECT_NO_THROW(DeriveUserCallbackParameter::From(holder));

    // When attempting to derive the user callback parameter from an invalid single instance holder
    // Then a precondition violation is expected
    holder.reset();
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DeriveUserCallbackParameter::From(holder));
}

TEST(DeriveUserCallbackParameter, FromOptionalInstanceHolder)
{
    class MyInstance
    {
    };

    // Given an optional instance holder of arbitrary type
    std::optional holder{MyInstance{}};
    ASSERT_TRUE(holder.has_value());

    // When attempting to derive the user callback parameter from it
    // Then no exception is expected
    EXPECT_NO_THROW(DeriveUserCallbackParameter::From(holder));

    // When attempting to derive the user callback parameter from an invalid single instance holder
    // Then a precondition violation is expected
    holder.reset();
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DeriveUserCallbackParameter::From(holder));
}

TEST(DeriveUserCallbackParameter, FromVariantInstanceHolder)
{
    class MyInstance
    {
    };

    class MyThrowingInstance
    {
      public:
        MyThrowingInstance() = default;
        MyThrowingInstance(MyThrowingInstance&&)
        {
            throw std::logic_error{"move attempt"};
        }
        MyThrowingInstance(const MyThrowingInstance&)
        {
            throw std::logic_error{"copy attempt"};
        }

        MyThrowingInstance& operator=(MyThrowingInstance&&)
        {
            throw std::logic_error{"move attempt"};
        }
        MyThrowingInstance& operator=(const MyThrowingInstance&)
        {
            throw std::logic_error{"copy attempt"};
        }
    };

    using VariantHolder = std::variant<std::optional<MyInstance>, std::optional<MyThrowingInstance>>;

    // Given a variant instance holder of arbitrary type
    VariantHolder holder{std::in_place_type<std::optional<MyInstance>>, std::in_place};

    // When attempting to derive the user callback parameter from it
    // Then no exception is expected
    EXPECT_NO_THROW(DeriveUserCallbackParameter::From(holder));

    // When attempting to derive the user callback parameter from a valueless variant instance holder
    EXPECT_THROW({ holder = std::optional<MyThrowingInstance>{std::in_place}; }, std::logic_error);
    ASSERT_TRUE(holder.valueless_by_exception());

    // Then a precondition violation is expected
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(DeriveUserCallbackParameter::From(holder));
}

TEST(ReusableProxyBuilderCallback, UserCallbackIsInvokedWhenProxyInstanceReceived)
{
    class MyInstance
    {
    };

    using Trait = ProxySpecTraits<Multiple<MyInstance>>;

    // Given a ReusableProxyBuilderCallback constructed with a UserCallback variant
    bool callback_called = false;
    typename Trait::UserCallbackVariant callback{std::in_place_type<typename Trait::UserCallback>,
                                                 [&callback_called](MyInstance&) {
                                                     callback_called = true;
                                                 }};

    score::concurrency::InterruptiblePromise<typename Trait::ContainerType> promise{};
    ReusableProxyBuilderCallback<Trait> unit{std::move(promise), std::move(callback)};

    // When the callback operator is invoked with a proxy instance
    unit(SingleInstanceHolderCreator<MyInstance>::Construct<MyInstance>());

    // Then the user callback is called
    EXPECT_TRUE(callback_called);
}

}  // namespace
}  // namespace score::mw::service::details
