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
#include "score/mw/com/impl/generic_skeleton.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/test/skeleton_binding_factory_mock_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

class GenericSkeletonTest : public ::testing::Test
{
  public:
    GenericSkeletonTest()
    {
        auto skeleton_binding_mock = std::make_unique<mock_binding::Skeleton>();
        skeleton_binding_mock_ = skeleton_binding_mock.get();

        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Return(ByMove(std::move(skeleton_binding_mock))));
    }

    RuntimeMockGuard runtime_mock_guard_{};
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    mock_binding::Skeleton* skeleton_binding_mock_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(GenericSkeletonTest, CreateWithInstanceSpecifier)
{
    auto instance_specifier = InstanceSpecifier::Create("a/b/c").value();
    auto instance_identifier = dummy_instance_identifier_builder_.CreateInstanceIdentifier();
    GenericSkeletonCreateParams create_params;

    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, ResolveInstanceIdentifier(instance_specifier))
        .WillOnce(Return(instance_identifier));

    auto skeleton_result = GenericSkeleton::Create(instance_specifier, create_params);
    ASSERT_TRUE(skeleton_result.has_value());
}

TEST_F(GenericSkeletonTest, CreateWithInstanceIdentifier)
{
    auto instance_identifier = dummy_instance_identifier_builder_.CreateInstanceIdentifier();
    GenericSkeletonCreateParams create_params;
    auto skeleton_result = GenericSkeleton::Create(instance_identifier, create_params);
    ASSERT_TRUE(skeleton_result.has_value());
}

TEST_F(GenericSkeletonTest, CreateWithEventsAndFields)
{
   auto instance_identifier = dummy_instance_identifier_builder_.CreateInstanceIdentifier();
    const DataTypeMetaInfo event_size_info{16, 8};
    // const DataTypeMetaInfo field_size_info{4, 4}; // commented out as field not implemented
    const std::string event_name = "MyEvent";
    // const std::string field_name = "MyField"; // commented out as field not implemented
    // const std::vector<std::uint8_t> initial_field_value = {1, 2, 3, 4}; // commented out as field not implemented

    // Mock event binding creation
    EXPECT_CALL(generic_skeleton_event_binding_factory_mock_guard_.factory_mock_, Create(_, event_name, event_size_info))
        .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericSkeletonEvent>())));

    // // Mock field binding creation // commented out as field not implemented
    // EXPECT_CALL(generic_skeleton_field_binding_factory_mock_guard_.factory_mock_, Create(_, field_name, field_size_info))
    //     .WillOnce(Return(ByMove(std::make_unique<mock_binding::GenericSkeletonField>())));

    GenericSkeletonCreateParams create_params;
    const std::vector<EventInfo> events_vec = {{event_name, event_size_info}};
    //const std::vector<FieldInfo> fields_vec = {{field_name, field_size_info, initial_field_value}};
    create_params.events = events_vec;
    //create_params.fields = fields_vec;

    auto skeleton_result = GenericSkeleton::Create(instance_identifier, create_params);
    ASSERT_TRUE(skeleton_result.has_value());

    auto& skeleton = skeleton_result.value();
    EXPECT_EQ(skeleton.GetEvents().size(), 1);
    // EXPECT_EQ(skeleton.GetFields().size(), 1); // commented out as field not implemented
    EXPECT_NE(skeleton.GetEvents().find(event_name), skeleton.GetEvents().cend());
    // EXPECT_NE(skeleton.GetFields().find(field_name), skeleton.GetFields().cend()); // commented out as field not implemented
}

TEST_F(GenericSkeletonTest, OfferAndStopOfferService)
{
    auto instance_identifier = dummy_instance_identifier_builder_.CreateInstanceIdentifier();
    GenericSkeletonCreateParams create_params; // Empty params for this test
    auto skeleton = GenericSkeleton::Create(instance_identifier, create_params).value();

    // Expect PrepareOffer to be called on the binding
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::result::Blank{}));
    // Expect OfferService to be called on the service discovery
    EXPECT_CALL(runtime_mock_guard_.service_discovery_mock_, OfferService(instance_identifier)).WillOnce(Return(score::result::Blank{}));

    auto offer_result = skeleton.OfferService();
    EXPECT_TRUE(offer_result.has_value());

    EXPECT_CALL(*skeleton_binding_mock_, PrepareStopOffer(_));
    EXPECT_CALL(runtime_mock_guard_.service_discovery_mock_, StopOfferService(instance_identifier));
    skeleton.StopOfferService();
}

TEST_F(GenericSkeletonTest, CreateFailsIfBindingFails)
{
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_)).WillOnce(Return(ByMove(nullptr)));
    GenericSkeletonCreateParams create_params;
    auto skeleton_result = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateInstanceIdentifier(), create_params);
    ASSERT_FALSE(skeleton_result.has_value());
    EXPECT_EQ(skeleton_result.error(), ComErrc::kBindingFailure);
}
} // namespace
} // namespace score::mw::com::impl