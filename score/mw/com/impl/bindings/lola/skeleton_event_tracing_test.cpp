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
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_test_resources.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_mock.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/bindings/lola/test/skeleton_event_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/tracing/configuration/service_element_instance_identifier_view.h"
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config_mock.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing_data.h"
#include "score/mw/com/impl/tracing/tracing_runtime_mock.h"

#include "score/filesystem/filesystem.h"

#include <score/optional.hpp>
#include <score/string_view.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{

template <typename SampleType>
class SkeletonEventAttorney
{
  public:
    explicit SkeletonEventAttorney(SkeletonEvent<SampleType>& skeleton_event) noexcept : skeleton_event_{skeleton_event}
    {
    }

    void SetQmDisconnect(const bool qm_disconnect_value)
    {
        skeleton_event_.qm_disconnect_ = qm_disconnect_value;
    }

    std::optional<EventDataControlComposite>& GetEventDataControlComposite()
    {
        return skeleton_event_.event_data_control_composite_;
    }

  private:
    SkeletonEvent<SampleType>& skeleton_event_;
};

namespace
{

using ::testing::_;
using ::testing::An;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

class SkeletonEventTracingFixture : public SkeletonEventFixture
{
  public:
    void SetUp() override
    {
        ON_CALL(tracing_runtime_mock_, IsTracingEnabled()).WillByDefault(Return(true));
    }

    impl::tracing::ServiceElementInstanceIdentifierView CreateServiceElementInstanceIdentifierView() const noexcept
    {
        const impl::tracing::ServiceElementIdentifierView service_element_identifier_view{
            service_type_name_, fake_event_name_, impl::tracing::ServiceElementType::EVENT};
        const impl::tracing::ServiceElementInstanceIdentifierView expected_service_element_instance_identifier_view{
            service_element_identifier_view,
            score::cpp::string_view{instance_specifier_.ToString().data(), instance_specifier_.ToString().size()}};
        return expected_service_element_instance_identifier_view;
    }

    EventSlotStatus::EventTimeStamp GetLastSendEventTimestamp(const SlotIndexType slot) noexcept
    {
        auto& event_data_control_compositve =
            SkeletonEventAttorney<test::TestSampleType>{*skeleton_event_}.GetEventDataControlComposite();
        EXPECT_TRUE(event_data_control_compositve.has_value());
        return event_data_control_compositve.value().GetEventSlotTimestamp(slot);
    }
};

using SkeletonEventTracingSendFixture = SkeletonEventTracingFixture;
TEST_F(SkeletonEventTracingSendFixture, SendCallsAreTracedWhenEnabled)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200106, SCR-18200787, SCR-18200533");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonEvent Send are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321, SCR-18200106). The Send trace points are traced with a "
                   "TracePointDataId which is the timestamp of the event slot timestamp (SCR-18200787, SCR-18200533).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const auto expected_service_element_instance_identifier_view = CreateServiceElementInstanceIdentifierView();

    const impl::tracing::ServiceElementTracingData service_element_tracing_data{0U, 1U};

    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.service_element_instance_identifier_view =
        expected_service_element_instance_identifier_view;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.service_element_tracing_data = service_element_tracing_data;

    const test::TestSampleType sample_data{10U};

    // Then a trace call relating to Send should be called containing the correct sample data and trace point id
    impl::tracing::ITracingRuntime::TracePointType trace_point_type{impl::tracing::SkeletonEventTracePointType::SEND};
    EXPECT_CALL(tracing_runtime_mock_,
                Trace(BindingType::kLoLa,
                      service_element_tracing_data,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      _,
                      _,
                      _,
                      _))
        .WillOnce(WithArgs<4, 6, 7>(
            Invoke([&sample_data, this](impl::tracing::ITracingRuntime::TracePointDataId trace_point_data_id,
                                        const void* data_ptr,
                                        std::size_t data_size) -> ResultBlank {
                const auto timestamp = GetLastSendEventTimestamp(0U);
                EXPECT_EQ(timestamp, trace_point_data_id);

                EXPECT_EQ(data_size, sizeof(test::TestSampleType));
                const auto actual_data_ptr = static_cast<const test::TestSampleType*>(data_ptr);
                EXPECT_EQ(*actual_data_ptr, sample_data);
                return {};
            })));

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // and then send is called
    auto tracing_handler =
        impl::tracing::CreateTracingSendCallback<test::TestSampleType>(expected_enabled_trace_points, *skeleton_event_);
    skeleton_event_->Send(sample_data, {std::move(tracing_handler)});
}

TEST_F(SkeletonEventTracingSendFixture, MultipleSendCallsUsesCorrectTracePointDataId)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200106, SCR-18200787, SCR-18200533");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonEvent Send are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321, SCR-18200106). The Send trace points are traced with a "
                   "TracePointDataId which is the timestamp of the event slot timestamp (SCR-18200787, SCR-18200533).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const auto expected_service_element_instance_identifier_view = CreateServiceElementInstanceIdentifierView();

    const impl::tracing::ServiceElementTracingData service_element_tracing_data{0U, 1U};

    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.service_element_instance_identifier_view =
        expected_service_element_instance_identifier_view;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.service_element_tracing_data = service_element_tracing_data;

    const test::TestSampleType sample_data[2] = {10U, 20U};

    // Then a trace call relating to Send should be called containing the correct sample data and trace point id
    impl::tracing::ITracingRuntime::TracePointType trace_point_type{impl::tracing::SkeletonEventTracePointType::SEND};
    SlotIndexType slot_index{0U};
    EXPECT_CALL(tracing_runtime_mock_,
                Trace(BindingType::kLoLa,
                      service_element_tracing_data,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      _,
                      _,
                      _,
                      _))
        .Times(2)
        .WillRepeatedly(WithArgs<4, 6, 7>(Invoke(
            [&sample_data, &slot_index, this](impl::tracing::ITracingRuntime::TracePointDataId trace_point_data_id,
                                              const void* data_ptr,
                                              std::size_t data_size) -> ResultBlank {
                const auto timestamp = GetLastSendEventTimestamp(slot_index);
                EXPECT_EQ(timestamp, trace_point_data_id);

                EXPECT_EQ(data_size, sizeof(test::TestSampleType));
                const auto actual_data_ptr = static_cast<const test::TestSampleType*>(data_ptr);
                EXPECT_EQ(*actual_data_ptr, sample_data[slot_index]);
                slot_index++;
                return {};
            })));

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // and then send is called twice
    auto tracing_handler =
        impl::tracing::CreateTracingSendCallback<test::TestSampleType>(expected_enabled_trace_points, *skeleton_event_);
    skeleton_event_->Send(sample_data[0], {std::move(tracing_handler)});

    auto tracing_handler_2 =
        impl::tracing::CreateTracingSendCallback<test::TestSampleType>(expected_enabled_trace_points, *skeleton_event_);
    skeleton_event_->Send(sample_data[1], {std::move(tracing_handler_2)});
}

TEST_F(SkeletonEventTracingSendFixture, SendCallsAreNotTracedWhenAllocateFails)
{
    RecordProperty("Verifies", "SCR-18216878");
    RecordProperty("Description", "The Trace point types for binding SkeletonEvent Send are correctly mapped.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const auto expected_service_element_instance_identifier_view = CreateServiceElementInstanceIdentifierView();

    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.service_element_instance_identifier_view =
        expected_service_element_instance_identifier_view;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.enable_send_with_allocate = true;

    const test::TestSampleType sample_data{10U};

    // Expecting that a trace call relating to Send is never called

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_, fake_event_name_, max_samples_, max_subscribers_, enforce_max_samples);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // When all of the available slots are used up such that the next call to Send will not be able to allocate a slot
    std::list<impl::SampleAllocateePtr<test::TestSampleType>> data_vector{};
    for (size_t i = 0; i < max_samples_; ++i)
    {
        auto slot_result = skeleton_event_->Allocate();
        ASSERT_TRUE(slot_result.has_value());
        data_vector.push_back(std::move(slot_result.value()));
    }

    // and then send is called
    auto tracing_handler =
        impl::tracing::CreateTracingSendCallback<test::TestSampleType>(expected_enabled_trace_points, *skeleton_event_);
    skeleton_event_->Send(sample_data, {std::move(tracing_handler)});
}

using SkeletonEventTracingSendWithAllocateFixture = SkeletonEventTracingFixture;
TEST_F(SkeletonEventTracingSendWithAllocateFixture, SendWithAllocateCallsAreTracedWhenEnabled)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200106, SCR-18200787, SCR-18200533");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonEvent Send with allocate are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321, SCR-18200106). The Send trace points are traced with a "
                   "TracePointDataId which is the timestamp of the event slot timestamp (SCR-18200787, SCR-18200533).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const auto expected_service_element_instance_identifier_view = CreateServiceElementInstanceIdentifierView();

    const impl::tracing::ServiceElementTracingData service_element_tracing_data{0U, 1U};

    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.service_element_instance_identifier_view =
        expected_service_element_instance_identifier_view;
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.service_element_tracing_data = service_element_tracing_data;

    const test::TestSampleType sample_data{10U};

    // then a trace call relating to Send should be called containing the correct sample data and trace point id
    impl::tracing::ITracingRuntime::TracePointType trace_point_type{
        impl::tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE};
    EXPECT_CALL(tracing_runtime_mock_,
                Trace(BindingType::kLoLa,
                      service_element_tracing_data,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      _,
                      _,
                      _,
                      _))
        .WillOnce(WithArgs<4, 6, 7>(
            Invoke([&sample_data, this](impl::tracing::ITracingRuntime::TracePointDataId trace_point_data_id,
                                        const void* data_ptr,
                                        std::size_t data_size) -> ResultBlank {
                const auto timestamp = GetLastSendEventTimestamp(0U);
                EXPECT_EQ(timestamp, trace_point_data_id);

                EXPECT_EQ(data_size, sizeof(test::TestSampleType));
                const auto actual_data_ptr = static_cast<const test::TestSampleType*>(data_ptr);
                EXPECT_EQ(*actual_data_ptr, sample_data);
                return {};
            })));

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // When allocating a slot
    auto slot_result = skeleton_event_->Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();

    // and assigning a value to the slot
    *slot = sample_data;

    // and then send is called
    auto tracing_handler = impl::tracing::CreateTracingSendWithAllocateCallback<test::TestSampleType>(
        expected_enabled_trace_points, *skeleton_event_);
    skeleton_event_->Send(std::move(slot), {std::move(tracing_handler)});
}
TEST_F(SkeletonEventTracingSendWithAllocateFixture, MultipleSendCallsUsesCorrectTracePointDataId)
{
    RecordProperty("Verifies", "SCR-18216878, SCR-18200105, SCR-18222321, SCR-18200106, SCR-18200787, SCR-18200533");
    RecordProperty("Description",
                   "The Trace point types for binding SkeletonEvent Send with allocate are correctly mapped "
                   "(SCR-18216878). The Send trace points are traced with a ShmDataChunkList "
                   "(SCR-18200105, SCR-18222321, SCR-18200106). The Send trace points are traced with a "
                   "TracePointDataId which is the timestamp of the event slot timestamp (SCR-18200787, SCR-18200533).");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    const bool enforce_max_samples{true};
    const auto expected_service_element_instance_identifier_view = CreateServiceElementInstanceIdentifierView();

    const impl::tracing::ServiceElementTracingData service_element_tracing_data{0U, 1U};

    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.service_element_instance_identifier_view =
        expected_service_element_instance_identifier_view;
    expected_enabled_trace_points.enable_send_with_allocate = true;
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.service_element_tracing_data = service_element_tracing_data;

    const test::TestSampleType sample_data[2] = {10U, 20U};

    // Then a trace call relating to Send should be called containing the correct sample data and trace point id
    impl::tracing::ITracingRuntime::TracePointType trace_point_type{
        impl::tracing::SkeletonEventTracePointType::SEND_WITH_ALLOCATE};
    SlotIndexType slot_index{0U};
    EXPECT_CALL(tracing_runtime_mock_,
                Trace(BindingType::kLoLa,
                      service_element_tracing_data,
                      expected_service_element_instance_identifier_view,
                      trace_point_type,
                      _,
                      _,
                      _,
                      _))
        .Times(2)
        .WillRepeatedly(WithArgs<4, 6, 7>(Invoke(
            [&sample_data, &slot_index, this](impl::tracing::ITracingRuntime::TracePointDataId trace_point_data_id,
                                              const void* data_ptr,
                                              std::size_t data_size) -> ResultBlank {
                const auto timestamp = GetLastSendEventTimestamp(slot_index);
                EXPECT_EQ(timestamp, trace_point_data_id);

                EXPECT_EQ(data_size, sizeof(test::TestSampleType));
                const auto actual_data_ptr = static_cast<const test::TestSampleType*>(data_ptr);
                EXPECT_EQ(*actual_data_ptr, sample_data[slot_index]);
                slot_index++;
                return {};
            })));

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // When allocating a slot
    auto slot_result = skeleton_event_->Allocate();
    ASSERT_TRUE(slot_result.has_value());
    auto& slot = slot_result.value();

    // and assigning a value to the slot
    *slot = sample_data[0];

    // and then send is called
    auto tracing_handler = impl::tracing::CreateTracingSendWithAllocateCallback<test::TestSampleType>(
        expected_enabled_trace_points, *skeleton_event_);
    skeleton_event_->Send(std::move(slot), {std::move(tracing_handler)});

    // and then allocating another slot
    auto slot_result_2 = skeleton_event_->Allocate();
    ASSERT_TRUE(slot_result_2.has_value());
    auto& slot_2 = slot_result_2.value();

    // and assigning a value to the slot
    *slot_2 = sample_data[1];
    auto tracing_handler_2 = impl::tracing::CreateTracingSendWithAllocateCallback<test::TestSampleType>(
        expected_enabled_trace_points, *skeleton_event_);

    // and then send is called again
    skeleton_event_->Send(std::move(slot_2), {std::move(tracing_handler_2)});
}

using SkeletonEventTracingPrepareOfferFixture = SkeletonEventTracingFixture;
TEST_F(SkeletonEventTracingPrepareOfferFixture, DisablingTracingWillNotRegisterTransactionLog)
{
    const bool enforce_max_samples{true};

    // Expecting that the TracingFilterConfig has no trace points enabled.
    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // Then a TransactionLog is not registered
    auto& transaction_log_set =
        GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM)->data_control.GetTransactionLogSet();
    const auto skeleton_transaction_log_result =
        TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog();
    ASSERT_FALSE(skeleton_transaction_log_result.has_value());
}

TEST_F(SkeletonEventTracingPrepareOfferFixture, EnablingSendTracingWillRegisterTransactionLog)
{
    const bool enforce_max_samples{true};

    // Expecting that the TracingFilterConfig has only the event Send trace point enabled.
    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // Then a TransactionLog is registered
    auto& transaction_log_set =
        GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM)->data_control.GetTransactionLogSet();
    const auto skeleton_transaction_log_result =
        TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog();
    ASSERT_TRUE(skeleton_transaction_log_result.has_value());
}

TEST_F(SkeletonEventTracingPrepareOfferFixture, EnablingSendWithAllocateTracingWillRegisterTransactionLog)
{
    const bool enforce_max_samples{true};

    // Expecting that the TracingFilterConfig has only the event Send with allocate trace point enabled.
    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send_with_allocate = true;

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // Then a TransactionLog is registered
    auto& transaction_log_set =
        GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM)->data_control.GetTransactionLogSet();
    const auto skeleton_transaction_log_result =
        TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog();
    ASSERT_TRUE(skeleton_transaction_log_result.has_value());
}

using SkeletonEventTracingPrepareStopOfferFixture = SkeletonEventTracingFixture;
TEST_F(SkeletonEventTracingPrepareStopOfferFixture, PrepareStopOfferWillRemoveRegisteredTransactionLog)
{
    const bool enforce_max_samples{true};

    // Expecting that the TracingFilterConfig has only the event Send trace point enabled.
    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // Then a TransactionLog is registered
    auto& transaction_log_set =
        GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM)->data_control.GetTransactionLogSet();
    ASSERT_TRUE(TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog().has_value());

    // and when calling PrepareStopOffer
    skeleton_event_->PrepareStopOffer();

    // Then the TransactionLog will be unregistered
    ASSERT_FALSE(TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog().has_value());
}

TEST_F(SkeletonEventTracingPrepareStopOfferFixture, PrepareStopOfferWillNotRemoveTransactionLogThatWasNotRegistered)
{
    const bool enforce_max_samples{true};

    // Expecting that the TracingFilterConfig has no trace points enabled.
    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};

    // Given a skeleton event in an offered service
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);

    // Given an offered event in an offered service
    skeleton_event_->PrepareOffer();

    // Then a TransactionLog is not registered, because expected_enabled_trace_points has no corresponding trace points
    // enabled
    auto& transaction_log_set =
        GetEventControl(fake_element_fq_id_, QualityType::kASIL_QM)->data_control.GetTransactionLogSet();
    ASSERT_FALSE(TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog().has_value());

    // and when calling PrepareStopOffer
    skeleton_event_->PrepareStopOffer();

    // Then the TransactionLog is still not registered
    ASSERT_FALSE(TransactionLogSetAttorney{transaction_log_set}.GetSkeletonTransactionLog().has_value());
}

TEST_F(SkeletonEventTracingPrepareStopOfferFixture, PrepareStopOfferWillCallClearTypeErasedSamplePtrs)
{
    // Given a SkeletonEventTracingData with a trace point enabled.
    impl::tracing::SkeletonEventTracingData expected_enabled_trace_points{};
    expected_enabled_trace_points.enable_send = true;
    expected_enabled_trace_points.service_element_tracing_data.service_element_range_start = 5U;
    expected_enabled_trace_points.service_element_tracing_data.number_of_service_element_tracing_slots = 10U;

    // Expecting that ClearTypeErasedSamplePtrs will be called on the TracingRuntime
    EXPECT_CALL(tracing_runtime_binding_mock_,
                ClearTypeErasedSamplePtrs(expected_enabled_trace_points.service_element_tracing_data));

    // Given a skeleton event in an offered service
    const bool enforce_max_samples{true};
    InitialiseSkeletonEvent(fake_element_fq_id_,
                            fake_event_name_,
                            max_samples_,
                            max_subscribers_,
                            enforce_max_samples,
                            expected_enabled_trace_points);
    skeleton_event_->PrepareOffer();

    // When calling PrepareStopOffer
    skeleton_event_->PrepareStopOffer();
}

}  // namespace
}  // namespace score::mw::com::impl::lola
