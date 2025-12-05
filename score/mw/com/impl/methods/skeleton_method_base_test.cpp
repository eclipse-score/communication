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
#include "score/mw/com/impl/methods/skeleton_method_base.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/impl/skeleton_base.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <memory>

namespace score::mw::com::impl
{

namespace
{
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

SkeletonBase MakeEmptySkeleton(std::string_view service_name)
{
    const ServiceTypeDeployment empty_type_deployment{score::cpp::blank{}};
    const ServiceIdentifierType service{make_ServiceIdentifierType(std::string{service_name})};
    const auto instance_specifier = InstanceSpecifier::Create(std::string{"/dummy_instance_specifier"}).value();
    const ServiceInstanceDeployment empty_instance_deployment{
        service, score::cpp::blank{}, QualityType::kASIL_QM, instance_specifier};

    return SkeletonBase(std::make_unique<mock_binding::Skeleton>(),
                        make_InstanceIdentifier(empty_instance_deployment, empty_type_deployment));
}

const auto kMethodName{"DummyMethod1"};

auto kEmptySkeleton1 = MakeEmptySkeleton("bla");
auto kEmptySkeleton2 = MakeEmptySkeleton("blabla");

class MyDummyMethod final : public SkeletonMethodBase
{
  public:
    MyDummyMethod() : SkeletonMethodBase{kEmptySkeleton1, kMethodName, nullptr} {}

    std::reference_wrapper<SkeletonBase> GetSkeletonReference()
    {
        return skeleton_base_;
    }
};

TEST(SkeletonMethodBaseTests, NotCopyable)
{
    static_assert(!std::is_copy_constructible<SkeletonMethodBase>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonMethodBase>::value, "Is wrongly copyable");
}

TEST(SkeletonMethodBaseTests, IsMoveable)
{
    static_assert(std::is_move_constructible<SkeletonMethodBase>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonMethodBase>::value, "Is not move assignable");
}

TEST(SkeletonMethodBase, UpdateSkeletonReferenceUpdatesTheReference)
{
    // Given a constructed SkeletonMethod with a valid reference to a Skeleton base class
    auto skeleton_method = std::make_unique<MyDummyMethod>();
    EXPECT_EQ(std::addressof(kEmptySkeleton1), std::addressof(skeleton_method->GetSkeletonReference().get()));

    // When UpdateSkeletonReference is called with a reference to a new Skeleton base class
    skeleton_method->UpdateSkeletonReference(kEmptySkeleton2);

    // Then the reference in the skeleton method is updated correctly
    EXPECT_EQ(std::addressof(kEmptySkeleton2), std::addressof(skeleton_method->GetSkeletonReference().get()));

    // and that the result is blank
}

}  // namespace

}  // namespace score::mw::com::impl
