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
#include "score/mw/com/types.h" /** \requirement SWS_CM_01013 */

#include "score/mw/com/impl/find_service_handle.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace
{

TEST(Types, ServiceHandleContainer)
{
  RecordProperty("lobster-tracing", "Lola.proxy_exec_less_creation");
  RecordProperty("Verifies", "SCR-21792716");
  RecordProperty("Description",
                 "Checks that ServiceHandleContainer exists and satisfies the "
                 "sequence container requirements. It verifies "
                 "that the ServiceHandleContainer is a std vector which is "
                 "guaranteed by the C++ standard to "
                 "satisfy the sequence container requirements "
                 "(https://en.cppreference.com/w/cpp/container/vector).");
  RecordProperty("TestType", "Requirements-based test");
  RecordProperty("Priority", "1");
  RecordProperty("DerivationTechnique", "Analysis of requirements");

  using TestType = std::uint32_t;
  static_assert(std::is_same<score::mw::com::ServiceHandleContainer<TestType>,
                             std::vector<TestType>>::value,
                "ServiceHandleContainer is not a std vector");
}

TEST(Types, FindServiceHandle)
{
    RecordProperty("Verifies", "SCR-21789762");
    RecordProperty("Description", "Checks for existence of FindServiceHandle");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t dummy_uid{10U};
    const score::mw::com::FindServiceHandle dummy_handle = score::mw::com::impl::make_FindServiceHandle(dummy_uid);
    static_cast<void>(dummy_handle);
}

TEST(Types, FindServiceHandler)
{
    RecordProperty("Verifies", "SCR-21791066");
    RecordProperty(
        "Description",
        "Checks for existence of FindServiceHandler and that it's publicly constructible with the correct signature.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    score::mw::com::FindServiceHandler<std::int32_t>{
        [](const score::mw::com::ServiceHandleContainer<std::int32_t>&, score::mw::com::FindServiceHandle) noexcept {}};
}

TEST(API, InstanceIdentifierContainerExists)
{
    RecordProperty("Verifies", "SCR-21784908");
    RecordProperty(
        "Description",
        "Checks that InstanceIdentifierContainer exists and satisfies the sequence container requirements. It verifies "
        "that the InstanceIdentifierContainer is a std vector which is guaranteed by the C++ standard to "
        "satisfy the sequence container requirements (https://en.cppreference.com/w/cpp/container/vector).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    score::mw::com::InstanceIdentifierContainer unit{};
    static_assert(
        std::is_same<score::mw::com::InstanceIdentifierContainer, std::vector<score::mw::com::InstanceIdentifier>>::value,
        "Container is not a sequence container");
}

TEST(API, EventReceiverHandlerExists)
{
    RecordProperty("Verifies", "SCR-14035954");
    RecordProperty("Description", "Checks whether the respective type exists.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    score::mw::com::EventReceiveHandler unit = []() noexcept {};
}

TEST(API, MethodCallProcessingModeExists)
{
    RecordProperty("Verifies", "SCR-17608357");
    RecordProperty("Description", "Checks whether the respective type exists.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    score::mw::com::MethodCallProcessingMode unit{score::mw::com::MethodCallProcessingMode::kEvent};
    static_cast<void>(unit);
}

TEST(API, SubscriptionStateExists)
{
    RecordProperty("Verifies", "SCR-14034769");
    RecordProperty("Description", "Checks whether the respective type exists.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_same<std::underlying_type<score::mw::com::SubscriptionState>::type, std::uint8_t>::value,
                  "Wrong underlying type");
    constexpr auto subscribed = score::mw::com::SubscriptionState::kSubscribed;
    constexpr auto not_subscribed = score::mw::com::SubscriptionState::kNotSubscribed;
    constexpr auto pending = score::mw::com::SubscriptionState::kSubscriptionPending;

    static_cast<void>(subscribed);
    static_cast<void>(not_subscribed);
    static_cast<void>(pending);
}

}  // namespace
