/********************************************************************************
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
 ********************************************************************************/

#include "score/mw/com/impl/bindings/lola/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_event_test_resources.h"
#include "score/mw/com/impl/sample_allocatee_guard.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace score::mw::com::impl::lola
{

class GenericSkeletonEventFixture : public SkeletonEventFixture
{
  protected:
    void SetUp() override
    {
        // Call base class SetUp which sets up mocks
        SkeletonEventFixture::SetUp();

        // Initialize the skeleton
        InitialiseSkeleton(GetValidInstanceIdentifier());

        // Prepare the skeleton offering a single service-element, so the fixed-capacity containers within
        // ServiceDataStorage are sized to hold the one generic event that the tests register below.
        SkeletonBinding::SkeletonEventBindings events{};
        SkeletonBinding::SkeletonFieldBindings fields{};
        events.emplace(fake_event_name_, mock_event_binding_);
        std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback{};
        std::ignore = skeleton_->PrepareOffer(events, fields, std::move(register_shm_object_trace_callback));
    }

    void CreateGenericSkeletonEvent(ElementFqId element_fq_id,
                                    const std::string& event_name,
                                    const std::size_t max_samples,
                                    const std::uint8_t max_subscribers,
                                    const bool enforce_max_samples)
    {
        const SkeletonEventProperties properties{max_samples, max_subscribers, enforce_max_samples};
        generic_skeleton_event_ = std::make_unique<GenericSkeletonEvent>(
            *skeleton_, event_name, properties, element_fq_id, size_info_, impl::tracing::SkeletonEventTracingData{});
    }

    std::unique_ptr<GenericSkeletonEvent> generic_skeleton_event_;
    const DataTypeMetaInfo size_info_{10U, 8U};
};

// TODO: Fix requirement linkage as soon as requirements are matured in S-CORE.

// Test: Construction
TEST_F(GenericSkeletonEventFixture, CanConstructAGenericSkeletonEvent)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description", "Checks that a GenericSkeletonEvent can be constructed.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // When constructing a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // Then a valid GenericSkeletonEvent is created
    EXPECT_NE(generic_skeleton_event_, nullptr);
}

// Test: GetBindingType
TEST_F(GenericSkeletonEventFixture, GetBindingType)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description", "Checks that GetBindingType returns kLoLa for GenericSkeletonEvent.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When requesting the binding type
    // Then we get kLoLa
    EXPECT_EQ(generic_skeleton_event_->GetBindingType(), BindingType::kLoLa);
}

// Test: GetSizeInfo
TEST_F(GenericSkeletonEventFixture, GetSizeInfo)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description", "Checks that GetSizeInfo returns correct size and alignment.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When requesting size info
    auto size_info = generic_skeleton_event_->GetSizeInfo();

    // Then we get the correct size and alignment
    EXPECT_EQ(size_info.first, size_info_.size);
    EXPECT_EQ(size_info.second, size_info_.alignment);
}

// Test: GetMaxSize
TEST_F(GenericSkeletonEventFixture, GetMaxSize)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description", "Checks that GetMaxSize returns correct maximum size.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When requesting max size
    auto max_size = generic_skeleton_event_->GetMaxSize();

    // Then we get the correct size
    EXPECT_EQ(max_size, size_info_.size);
}

// Test: PrepareOffer
TEST_F(GenericSkeletonEventFixture, PrepareOffer)
{
    RecordProperty("Verifies", "SCR-14035184");
    RecordProperty("Description", "Checks that PrepareOffer successfully initializes the event.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    auto result = generic_skeleton_event_->PrepareOffer();

    // Then the operation succeeds
    EXPECT_TRUE(result.has_value());
}

// Test: Allocate - before PrepareOffer
TEST_F(GenericSkeletonEventFixture, CannotAllocateBeforePrepareOffer)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-17434933");
    RecordProperty("Description", "Checks that allocation fails before PrepareOffer.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent that has NOT been offered
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When trying to allocate without PrepareOffer
    auto ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});

    // Then allocation fails
    EXPECT_FALSE(ptr.has_value());
}

// Test: Allocate - after PrepareOffer
TEST_F(GenericSkeletonEventFixture, CanAllocateAfterPrepareOffer)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-17434933");
    RecordProperty("Description", "Checks that allocation succeeds after PrepareOffer.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    std::ignore = generic_skeleton_event_->PrepareOffer();

    // And allocating a sample
    auto ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});

    // Then allocation succeeds
    EXPECT_TRUE(ptr.has_value());
}

// Test: Multiple allocations
TEST_F(GenericSkeletonEventFixture, MultipleAllocationsWork)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-17434933");
    RecordProperty("Description", "Checks that multiple allocations work correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    std::ignore = generic_skeleton_event_->PrepareOffer();

    // And allocating multiple samples
    std::vector<impl::SampleAllocateePtr<void>> pointers;
    for (std::size_t i = 0; i < max_samples; ++i)
    {
        auto ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});
        EXPECT_TRUE(ptr.has_value());
        pointers.push_back(std::move(ptr).value());
    }

    // Then all allocations succeeded
    EXPECT_EQ(pointers.size(), max_samples);
}

// Test: Allocation fails when slots are full
TEST_F(GenericSkeletonEventFixture, AllocationFailsWhenSlotsFull)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-17434933, SCR-5899090");
    RecordProperty("Description", "Checks that allocation fails when all slots are used.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{3U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent with limited slots
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    std::ignore = generic_skeleton_event_->PrepareOffer();

    // And allocating all available slots
    std::vector<impl::SampleAllocateePtr<void>> pointers;
    for (std::size_t i = 0; i < max_samples; ++i)
    {
        auto ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});
        EXPECT_TRUE(ptr.has_value());
        pointers.push_back(std::move(ptr).value());
    }

    // When trying to allocate beyond max_samples on ASIL-B
    EXPECT_CALL(service_discovery_mock_, StopOfferService(testing::_, IServiceDiscovery::QualityTypeSelector::kAsilQm))
        .Times(1);

    auto overflow_ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});

    // Then allocation fails
    EXPECT_FALSE(overflow_ptr.has_value());
}

// Test: Send after allocation
TEST_F(GenericSkeletonEventFixture, CanSendAfterAllocate)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description", "Checks that Send works with allocated samples.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    std::ignore = generic_skeleton_event_->PrepareOffer();

    // And allocating a sample
    auto ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(ptr.has_value());

    auto sample = std::move(ptr).value();

    // When sending the sample (moving the sample since Send takes by value)
    auto result = generic_skeleton_event_->Send(std::move(sample));

    // Then the send succeeds
    EXPECT_TRUE(result.has_value());
}

// Test: Notify consumers
TEST_F(GenericSkeletonEventFixture, CanNotifyConsumers)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description", "Checks that Notify works correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    std::ignore = generic_skeleton_event_->PrepareOffer();

    // When calling Notify (without handlers registered, this should succeed)
    auto result = generic_skeleton_event_->Notify();

    // Then the notify succeeds
    EXPECT_TRUE(result.has_value());
}

// Test: Notify triggers message passing when handlers are registered
TEST_F(GenericSkeletonEventFixture, NotifyCallsMessagePassingWithRegisteredHandlers)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description",
                   "Checks that Notify forwards event updates to message passing when receive handlers are present.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // Expect that EventNotificationExistenceChangedCallbacks are registered for both ASIL levels at message_passing
    EXPECT_CALL(
        message_passing_mock_,
        RegisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, fake_element_fq_id_, testing::_))
        .WillOnce(testing::Invoke(
            [](QualityType, ElementFqId, IMessagePassingService::HandlerStatusChangeCallback has_handlers_callback) {
                has_handlers_callback(true);
            }));
    EXPECT_CALL(
        message_passing_mock_,
        RegisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, fake_element_fq_id_, testing::_))
        .WillOnce(testing::Invoke(
            [](QualityType, ElementFqId, IMessagePassingService::HandlerStatusChangeCallback has_handlers_callback) {
                has_handlers_callback(true);
            }));

    // When offering the event
    std::ignore = generic_skeleton_event_->PrepareOffer();
    // Expect that NotifyEvent gets called at message-passing for both ASIL-levels
    EXPECT_CALL(message_passing_mock_, NotifyEvent(QualityType::kASIL_QM, fake_element_fq_id_));
    EXPECT_CALL(message_passing_mock_, NotifyEvent(QualityType::kASIL_B, fake_element_fq_id_));
    // When Notify() gets called on the GenericSkeletonEvent
    auto result = generic_skeleton_event_->Notify();

    // Then message passing is notified for both quality levels, and Notify() completes without errors.
    EXPECT_TRUE(result.has_value());
}

// Test: PrepareStopOffer
TEST_F(GenericSkeletonEventFixture, PrepareStopOffer)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description", "Checks that PrepareStopOffer cleans up resources.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When calling PrepareOffer
    std::ignore = generic_skeleton_event_->PrepareOffer();

    // And allocating a sample
    auto ptr = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(ptr.has_value());

    // When calling PrepareStopOffer
    generic_skeleton_event_->PrepareStopOffer();

    // Then no error occurs
    EXPECT_TRUE(true);  // PrepareStopOffer is void, just ensure it completes
}

// Test: SetSkeletonEventTracingData
TEST_F(GenericSkeletonEventFixture, SetSkeletonEventTracingData)
{
    RecordProperty("Verifies", "SCR-21840368");
    RecordProperty("Description", "Checks that SetSkeletonEventTracingData works correctly.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When setting tracing data
    impl::tracing::SkeletonEventTracingData tracing_data{};
    tracing_data.enable_send = true;

    generic_skeleton_event_->SetSkeletonEventTracingData(tracing_data);

    // Then no error occurs
    EXPECT_TRUE(true);  // SetSkeletonEventTracingData is void, just ensure it completes
}

// Test: Full lifecycle
TEST_F(GenericSkeletonEventFixture, FullEventLifecycle)
{
    RecordProperty("Verifies", "SCR-21840368, SCR-17434933, SCR-5899090");
    RecordProperty("Description", "Checks complete event lifecycle: Offer -> Allocate -> Send -> StopOffer.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const std::size_t max_samples{5U};
    const std::uint8_t max_subscribers{3U};

    // Given a GenericSkeletonEvent
    CreateGenericSkeletonEvent(
        fake_element_fq_id_, fake_event_name_, max_samples, max_subscribers, enforce_max_samples);

    // When offering the event
    auto offer_result = generic_skeleton_event_->PrepareOffer();
    EXPECT_TRUE(offer_result.has_value());

    // And allocating a sample
    auto alloc_result = generic_skeleton_event_->Allocate(SampleAllocateeGuard{});
    EXPECT_TRUE(alloc_result.has_value());

    auto sample = std::move(alloc_result).value();

    // And sending the sample
    auto send_result = generic_skeleton_event_->Send(std::move(sample));
    EXPECT_TRUE(send_result.has_value());

    // And notifying consumers
    auto notify_result = generic_skeleton_event_->Notify();
    EXPECT_TRUE(notify_result.has_value());

    // And stopping the offer
    generic_skeleton_event_->PrepareStopOffer();

    // Then the complete lifecycle succeeds
}

}  // namespace score::mw::com::impl::lola
