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
// Note. It should be ensured that no other mw/com or Lola headers are included. This file tests that all required
// classes are accessible by only including types.h
#include "score/mw/com/types.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <type_traits>

namespace
{

TEST(TypesAccessibilityTest, TypesExistInCorrectNamespaceWithCorrectInclude)
{
    RecordProperty("Verifies", "SCR-21778053");
    RecordProperty(
        "Description",
        "Checks that all the required classes are accessible in the mw::com namespace by including mw/com/types.h");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_enum_v<score::mw::com::SubscriptionState>, "SubscriptionState does not exist");
    static_assert(std::is_enum_v<score::mw::com::MethodCallProcessingMode>, "MethodCallProcessingMode does not exist");
    static_assert(std::is_class_v<score::mw::com::EventReceiveHandler>, "EventReceiveHandler does not exist");
    static_assert(std::is_class_v<score::mw::com::ServiceHandleContainer<std::uint32_t>>,
                  "ServiceHandleContainer does not exist");
    static_assert(std::is_class_v<score::mw::com::SamplePtr<std::uint32_t>>, "SamplePtr does not exist");
    static_assert(std::is_class_v<score::mw::com::SampleAllocateePtr<std::uint32_t>>,
                  "SampleAllocateePtr does not exist");
    static_assert(std::is_class_v<score::mw::com::InstanceSpecifier>, "InstanceSpecifier does not exist");
    static_assert(std::is_class_v<score::mw::com::InstanceIdentifier>, "InstanceIdentifier does not exist");
    static_assert(std::is_class_v<score::mw::com::InstanceIdentifierContainer>,
                  "InstanceIdentifierContainer does not exist");
    static_assert(std::is_class_v<score::mw::com::FindServiceHandle>, "FindServiceHandle does not exist");
    static_assert(std::is_class_v<score::mw::com::FindServiceHandler<std::uint32_t>>,
                  "FindServiceHandler does not exist");
}

TEST(TypesAccessibilityTest, GenericProxyExistInCorrectNamespaceWithCorrectInclude)
{
    RecordProperty("Verifies", "SCR-14005667");
    RecordProperty(
        "Description",
        "Checks that the GenericProxy class is accessible in the mw::com namespace by including mw/com/types.h");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    static_assert(std::is_class_v<score::mw::com::GenericProxy>, "GenericProxy does not exist");
}

}  // namespace
