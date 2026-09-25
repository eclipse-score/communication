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
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"

#include "score/mw/com/impl/bindings/someip/in_process_transport.h"
#include "score/mw/com/impl/bindings/someip/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/bindings/someip/test/vehicle_status.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/raw_wire_representation_fundamentals.h"
#include "score/mw/com/impl/sample_serialization_spec.h"
#include "score/mw/com/impl/serialization_sink.h"
#include "score/mw/com/impl/serialize_sample_callback.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

namespace score::mw::com::impl::someip
{
namespace
{

using TestSampleType = std::uint32_t;

constexpr SomeIpServiceId kServiceId{1U};
constexpr SomeIpEventId kEventId{2U};
constexpr SomeIpServiceInstanceId::InstanceId kInstanceId{3U};
constexpr std::size_t kNumberOfSlots{2U};

const ElementFqId kEventFqId{kServiceId, kEventId, kInstanceId, ServiceElementType::EVENT};
const memory::DataTypeSizeInfo kSampleSizeInfo{sizeof(TestSampleType), alignof(TestSampleType)};

/// \brief Owns everything a someip::Skeleton needs to be constructed from a (SOME/IP) InstanceIdentifier.
class DeploymentStore final
{
  public:
    DeploymentStore()
        : instance_specifier_{InstanceSpecifier::Create("/someip_skeleton_event_test").value()},
          service_identifier_{make_ServiceIdentifierType("SomeIpSkeletonEventTestService", 1U, 0U)},
          service_type_deployment_{std::make_unique<ServiceTypeDeployment>(
              SomeIpServiceTypeDeployment{kServiceId, {{"DummyEvent", kEventId}}})},
          service_instance_deployment_{std::make_unique<ServiceInstanceDeployment>(
              service_identifier_,
              SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{kInstanceId}},
              QualityType::kASIL_QM,
              instance_specifier_)}
    {
    }

    InstanceIdentifier GetInstanceIdentifier() const noexcept
    {
        return make_InstanceIdentifier(*service_instance_deployment_, *service_type_deployment_);
    }

  private:
    InstanceSpecifier instance_specifier_;
    ServiceIdentifierType service_identifier_;
    std::unique_ptr<ServiceTypeDeployment> service_type_deployment_;
    std::unique_ptr<ServiceInstanceDeployment> service_instance_deployment_;
};

class SomeIpSkeletonEventFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        skeleton_ = std::make_unique<Skeleton>(deployment_store_.GetInstanceIdentifier(), transport_);
        unit_ = std::make_unique<SkeletonEvent>(*skeleton_,
                                                kEventFqId,
                                                "DummyEvent",
                                                kSampleSizeInfo,
                                                SkeletonEventProperties{kNumberOfSlots, 1U, true},
                                                impl::tracing::SkeletonEventTracingData{});
    }

    InitializeSampleCallback MakeInitializeSampleCallback() const noexcept
    {
        return InitializeSampleCallback{[](void* const sample_ptr) noexcept {
            score::cpp::ignore = new (sample_ptr) TestSampleType{};
        }};
    }

    /// \brief Wraps a serializer into a specification, sized generously so that tests which deliberately produce a
    ///        payload differing from sizeof(TestSampleType) are not rejected by the buffer bound.
    static SampleSerializationSpec MakeSpec(SerializeSampleCallback callback,
                                            const std::size_t max_serialized_size = 64U) noexcept
    {
        return SampleSerializationSpec{std::move(callback), max_serialized_size};
    }

    /// \brief A serializer whose output differs from the raw object representation, so that tests can prove the
    ///        transmitted bytes really went through the serializer.
    static SerializeSampleCallback MakeByteSwappingSerializer() noexcept
    {
        return SerializeSampleCallback{[](const void* const sample_ptr, ISerializationSink& sink) noexcept {
            std::array<std::byte, sizeof(TestSampleType)> bytes{};
            std::memcpy(bytes.data(), sample_ptr, sizeof(TestSampleType));
            std::reverse(bytes.begin(), bytes.end());
            score::cpp::ignore = sink.Write(bytes.data(), bytes.size());
        }};
    }

    TestSampleType GetLastSentValue() const noexcept
    {
        const auto payload = transport_.GetLastSentPayload(kEventFqId);
        EXPECT_EQ(payload.size(), sizeof(TestSampleType));
        TestSampleType value{};
        std::memcpy(&value, payload.data(), sizeof(TestSampleType));
        return value;
    }

    InProcessTransport transport_{};
    DeploymentStore deployment_store_{};
    std::unique_ptr<Skeleton> skeleton_{};
    std::unique_ptr<SkeletonEvent> unit_{};
};

/// \brief Sends a value by copy, the way the binding independent SkeletonEvent<T> does it.
/// \details SkeletonEventBinding does not offer a copy-Send overload taking a raw pointer; the strongly typed layer
///          allocates a slot, assigns the value into it and sends that slot instead. Reproducing those steps here
///          keeps the tests exercising the same path a real send takes.
template <typename SampleType>
Result<void> SendByCopy(SkeletonEvent& event, const SampleType& value) noexcept
{
    auto allocate_result = event.Allocate(SampleAllocateeGuard{});
    if (!allocate_result.has_value())
    {
        return MakeUnexpected(ComErrc::kSampleAllocationFailure, "Could not allocate slot");
    }
    auto allocated_slot = std::move(allocate_result).value();
    *static_cast<SampleType*>(allocated_slot.Get()) = value;
    return event.Send(std::move(allocated_slot), std::nullopt);
}

TEST_F(SomeIpSkeletonEventFixture, GetBindingTypeReturnsSomeIp)
{
    EXPECT_EQ(unit_->GetBindingType(), BindingType::kSomeIp);
}

TEST_F(SomeIpSkeletonEventFixture, GetEventDataTypeSizeInfoReturnsConfiguredSizeInfo)
{
    EXPECT_EQ(unit_->GetEventDataTypeSizeInfo().Size(), kSampleSizeInfo.Size());
    EXPECT_EQ(unit_->GetEventDataTypeSizeInfo().Alignment(), kSampleSizeInfo.Alignment());
}

TEST_F(SomeIpSkeletonEventFixture, AllocateBeforeOfferFails)
{
    const auto result = unit_->Allocate(SampleAllocateeGuard{});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kNotOffered);
}

TEST_F(SomeIpSkeletonEventFixture, PrepareOfferOffersTheEventOnTheTransport)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // The transport only accepts sends for offered events, so a successful send proves the offer reached it.
    const TestSampleType value{42U};
    EXPECT_TRUE(SendByCopy(*unit_, value).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, PrepareOfferInitializesEverySlot)
{
    std::size_t number_of_initialized_slots{0U};
    ASSERT_TRUE(unit_
                    ->PrepareOffer(InitializeSampleCallback{[&number_of_initialized_slots](void* const sample_ptr) {
                        ++number_of_initialized_slots;
                        score::cpp::ignore = new (sample_ptr) TestSampleType{};
                    }})
                    .has_value());

    EXPECT_EQ(number_of_initialized_slots, kNumberOfSlots);
}

TEST_F(SomeIpSkeletonEventFixture, SendWithCopyTransmitsThePayload)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0xDEADBEEFU};

    ASSERT_TRUE(SendByCopy(*unit_, value).has_value());

    EXPECT_EQ(GetLastSentValue(), value);
}

TEST_F(SomeIpSkeletonEventFixture, SendWithAllocateTransmitsThePayload)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(allocate_result.has_value());
    auto sample = std::move(allocate_result).value();
    *static_cast<TestSampleType*>(sample.Get()) = 7U;

    ASSERT_TRUE(unit_->Send(std::move(sample), std::nullopt).has_value());

    EXPECT_EQ(GetLastSentValue(), 7U);
}

TEST_F(SomeIpSkeletonEventFixture, SendWithCopyDoesNotExhaustSlots)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // More sends than there are slots: every Send() has to return its slot, otherwise this runs out of slots.
    for (TestSampleType value = 0U; value < (kNumberOfSlots * 3U); ++value)
    {
        ASSERT_TRUE(SendByCopy(*unit_, value).has_value()) << "at value " << value;
    }

    EXPECT_EQ(GetLastSentValue(), (kNumberOfSlots * 3U) - 1U);
}

TEST_F(SomeIpSkeletonEventFixture, AllocateFailsWhenAllSlotsAreHeldByTheUser)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    std::vector<impl::SampleAllocateePtr<void>> held_samples{};
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
        ASSERT_TRUE(allocate_result.has_value());
        held_samples.push_back(std::move(allocate_result).value());
    }

    EXPECT_FALSE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, DestroyingAnUnsentSampleReturnsItsSlot)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    {
        std::vector<impl::SampleAllocateePtr<void>> held_samples{};
        for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
        {
            auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
            ASSERT_TRUE(allocate_result.has_value());
            held_samples.push_back(std::move(allocate_result).value());
        }
        ASSERT_FALSE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
    }

    EXPECT_TRUE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, SendReturnsExactlyOneSlot)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // Hold every slot but one, then send the remaining one. If Send() returned its slot more than once, one of the
    // still-held slots would be wrongly marked free and the Allocate() below would wrongly succeed.
    std::vector<impl::SampleAllocateePtr<void>> held_samples{};
    for (std::size_t slot = 0U; slot < (kNumberOfSlots - 1U); ++slot)
    {
        auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
        ASSERT_TRUE(allocate_result.has_value());
        held_samples.push_back(std::move(allocate_result).value());
    }

    const TestSampleType value{42U};
    ASSERT_TRUE(SendByCopy(*unit_, value).has_value());

    // Exactly one slot became free again, so exactly one further allocation must succeed.
    auto first_allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(first_allocate_result.has_value());
    EXPECT_FALSE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, SendReturnsItsSlotEvenWhenTheTransportFails)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    // Withdrawing the event on the transport directly (without going through PrepareStopOffer) makes the next
    // SendEvent() fail while the event itself still considers itself offered.
    transport_.StopOfferEvent(kEventFqId);

    const TestSampleType value{42U};
    ASSERT_FALSE(SendByCopy(*unit_, value).has_value());

    // All slots must still be available, i.e. the failed send must not have leaked its slot.
    for (std::size_t slot = 0U; slot < kNumberOfSlots; ++slot)
    {
        EXPECT_TRUE(unit_->Allocate(SampleAllocateeGuard{}).has_value()) << "at slot " << slot;
    }
}

TEST_F(SomeIpSkeletonEventFixture, PrepareStopOfferWithdrawsTheEvent)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    unit_->PrepareStopOffer();

    const TestSampleType value{42U};
    EXPECT_FALSE(SendByCopy(*unit_, value).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, ReOfferingReusesTheExistingStorageWithoutReInitializingSlots)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    unit_->PrepareStopOffer();

    std::size_t number_of_initialized_slots{0U};
    ASSERT_TRUE(unit_
                    ->PrepareOffer(InitializeSampleCallback{[&number_of_initialized_slots](void* const sample_ptr) {
                        ++number_of_initialized_slots;
                        score::cpp::ignore = new (sample_ptr) TestSampleType{};
                    }})
                    .has_value());

    EXPECT_EQ(number_of_initialized_slots, 0U);
}

TEST_F(SomeIpSkeletonEventFixture, NotifyBeforeOfferFails)
{
    EXPECT_FALSE(unit_->Notify().has_value());
}

TEST_F(SomeIpSkeletonEventFixture, NotifyIsForwardedToTheTransport)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    ASSERT_TRUE(unit_->Notify().has_value());

    EXPECT_EQ(transport_.GetNotificationCount(kEventFqId), 1U);
}

TEST_F(SomeIpSkeletonEventFixture, GetLatestSampleIsNotSupported)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    EXPECT_FALSE(unit_->GetLatestSample(QualityType::kASIL_QM).has_value());
}

TEST_F(SomeIpSkeletonEventFixture, SettingTheReceiveHandlerRegistrationChangedHandlerReportsCurrentState)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    std::optional<bool> reported_state{};
    ASSERT_TRUE(unit_
                    ->SetReceiveHandlerRegistrationChangedHandler(
                        ReceiveHandlerRegistrationChangedCallback{[&reported_state](const bool has_handlers) noexcept {
                            reported_state = has_handlers;
                        }})
                    .has_value());

    ASSERT_TRUE(reported_state.has_value());
    EXPECT_FALSE(reported_state.value());

    ASSERT_TRUE(transport_.Subscribe(kEventFqId).has_value());
    ASSERT_TRUE(unit_
                    ->SetReceiveHandlerRegistrationChangedHandler(
                        ReceiveHandlerRegistrationChangedCallback{[&reported_state](const bool has_handlers) noexcept {
                            reported_state = has_handlers;
                        }})
                    .has_value());
    EXPECT_TRUE(reported_state.value());
}

TEST_F(SomeIpSkeletonEventFixture, UnsettingTheReceiveHandlerRegistrationChangedHandlerSucceeds)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    EXPECT_TRUE(unit_->UnsetReceiveHandlerRegistrationChangedHandler().has_value());
}

/// \brief Without a serializer the binding has to keep transmitting the raw object representation, which is the
///        situation for a GenericSkeletonEvent: it is already type-erased and therefore hands over no serializer.
TEST_F(SomeIpSkeletonEventFixture, SendWithoutSerializerTransmitsTheRawObjectRepresentation)
{
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0x12345678U};

    ASSERT_TRUE(SendByCopy(*unit_, value).has_value());

    const auto payload = transport_.GetLastSentPayload(kEventFqId);
    ASSERT_EQ(payload.size(), sizeof(TestSampleType));
    EXPECT_EQ(GetLastSentValue(), value);
}

TEST_F(SomeIpSkeletonEventFixture, SerializerIsUsedOnTheSendWithCopyPath)
{
    unit_->SetSampleSerialization(MakeSpec(MakeByteSwappingSerializer()));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0x12345678U};

    ASSERT_TRUE(SendByCopy(*unit_, value).has_value());

    EXPECT_EQ(GetLastSentValue(), TestSampleType{0x78563412U});
}

TEST_F(SomeIpSkeletonEventFixture, SerializerIsUsedOnTheSendWithAllocatePath)
{
    unit_->SetSampleSerialization(MakeSpec(MakeByteSwappingSerializer()));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    auto allocate_result = unit_->Allocate(SampleAllocateeGuard{});
    ASSERT_TRUE(allocate_result.has_value());
    auto sample = std::move(allocate_result).value();
    *static_cast<TestSampleType*>(sample.Get()) = TestSampleType{0x12345678U};

    ASSERT_TRUE(unit_->Send(std::move(sample), std::nullopt).has_value());

    EXPECT_EQ(GetLastSentValue(), TestSampleType{0x78563412U});
}

/// \brief The serialized representation is what reaches the transport, so its size may differ from the size of the
///        in-memory sample. This is what a real SOME/IP wire format (e.g. variable length members) requires.
TEST_F(SomeIpSkeletonEventFixture, SerializerMayProduceAPayloadOfADifferentSize)
{
    static constexpr std::array<std::byte, 3U> kSerializedForm{std::byte{0xAAU}, std::byte{0xBBU}, std::byte{0xCCU}};
    unit_->SetSampleSerialization(MakeSpec(SerializeSampleCallback{[](const void*, ISerializationSink& sink) noexcept {
        score::cpp::ignore = sink.Write(kSerializedForm.data(), kSerializedForm.size());
    }}));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0U};

    ASSERT_TRUE(SendByCopy(*unit_, value).has_value());

    const auto payload = transport_.GetLastSentPayload(kEventFqId);
    ASSERT_EQ(payload.size(), kSerializedForm.size());
    EXPECT_TRUE(std::equal(payload.cbegin(), payload.cend(), kSerializedForm.cbegin()));
}

/// \brief A serializer which appends in several steps must produce one contiguous payload.
TEST_F(SomeIpSkeletonEventFixture, SerializerCanWriteTheSampleInMultipleChunks)
{
    unit_->SetSampleSerialization(
        MakeSpec(SerializeSampleCallback{[](const void* const sample_ptr, ISerializationSink& sink) noexcept {
            const auto* const bytes = static_cast<const std::byte*>(sample_ptr);
            for (std::size_t i = 0U; i < sizeof(TestSampleType); ++i)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) iterating the object representation
                score::cpp::ignore = sink.Write(bytes + i, 1U);
            }
        }}));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0x0BADF00DU};

    ASSERT_TRUE(SendByCopy(*unit_, value).has_value());

    EXPECT_EQ(GetLastSentValue(), value);
}

/// \brief The buffer is reused across sends, so it must not accumulate the payload of previous sends.
TEST_F(SomeIpSkeletonEventFixture, ConsecutiveSendsDoNotAccumulateTheSerializationBuffer)
{
    unit_->SetSampleSerialization(MakeSpec(MakeByteSwappingSerializer()));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());

    for (TestSampleType value = 1U; value <= 3U; ++value)
    {
        ASSERT_TRUE(SendByCopy(*unit_, value).has_value());
        const auto payload = transport_.GetLastSentPayload(kEventFqId);
        ASSERT_EQ(payload.size(), sizeof(TestSampleType));
    }
}

TEST_F(SomeIpSkeletonEventFixture, SendFailsWhenSerializationFails)
{
    unit_->SetSampleSerialization(MakeSpec(SerializeSampleCallback{[](const void*, ISerializationSink& sink) noexcept {
        // A nullptr write is rejected by the sink and marks the serialization as failed.
        score::cpp::ignore = sink.Write(nullptr, 1U);
    }}));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{42U};

    const auto result = SendByCopy(*unit_, value);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

/// \brief A failed send must not leak the slot it allocated, otherwise the event runs out of slots.
TEST_F(SomeIpSkeletonEventFixture, FailedSerializationReturnsTheSlot)
{
    unit_->SetSampleSerialization(MakeSpec(SerializeSampleCallback{[](const void*, ISerializationSink& sink) noexcept {
        score::cpp::ignore = sink.Write(nullptr, 1U);
    }}));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{42U};

    for (std::size_t i = 0U; i < (kNumberOfSlots + 1U); ++i)
    {
        EXPECT_FALSE(SendByCopy(*unit_, value).has_value());
    }

    // All slots must still be available after the failed sends.
    EXPECT_TRUE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

/// \brief End to end proof of the whole chain: the sample handed to Send() reaches the transport in its SOME/IP wire
///        representation, produced by SampleWireFormat -> SomeIpWriter -> SomeIpSerializer, and not as the raw
///        object representation of the slot.
TEST_F(SomeIpSkeletonEventFixture, SendTransmitsTheWireRepresentationProducedByTheSampleWireFormat)
{
    using VehicleStatus = test::VehicleStatus;
    using WireFormat = impl::SampleWireFormat<VehicleStatus>;

    SkeletonEvent vehicle_event{*skeleton_,
                                kEventFqId,
                                "DummyEvent",
                                memory::DataTypeSizeInfo{sizeof(VehicleStatus), alignof(VehicleStatus)},
                                SkeletonEventProperties{kNumberOfSlots, 1U, true},
                                impl::tracing::SkeletonEventTracingData{}};
    vehicle_event.SetSampleSerialization(impl::SampleSerializationSpec{
        SerializeSampleCallback{[](const void* const sample_ptr, ISerializationSink& sink) noexcept {
            score::cpp::ignore = WireFormat::Serialize(*static_cast<const VehicleStatus*>(sample_ptr), sink);
        }},
        WireFormat::kMaxSerializedSize});

    ASSERT_TRUE(vehicle_event
                    .PrepareOffer(InitializeSampleCallback{[](void* const sample_ptr) noexcept {
                        score::cpp::ignore = new (sample_ptr) VehicleStatus{};
                    }})
                    .has_value());

    const VehicleStatus sample{0x11223344U, 0x0064U, 0x03U, test::WheelSpeeds{1U, 2U, 3U, 4U}};
    ASSERT_TRUE(SendByCopy(vehicle_event, sample).has_value());

    const auto payload = transport_.GetLastSentPayload(kEventFqId);
    const std::array<std::byte, 15U> expected{std::byte{0x11},
                                              std::byte{0x22},
                                              std::byte{0x33},
                                              std::byte{0x44},
                                              std::byte{0x00},
                                              std::byte{0x64},
                                              std::byte{0x03},
                                              std::byte{0x00},
                                              std::byte{0x01},
                                              std::byte{0x00},
                                              std::byte{0x02},
                                              std::byte{0x00},
                                              std::byte{0x03},
                                              std::byte{0x00},
                                              std::byte{0x04}};

    // 15 bytes on the wire instead of the 16 the object occupies in memory: no padding byte, network byte order.
    ASSERT_EQ(payload.size(), expected.size());
    ASSERT_LT(payload.size(), sizeof(VehicleStatus));
    EXPECT_TRUE(std::equal(payload.cbegin(), payload.cend(), expected.cbegin()));
}

/// \brief A serializer producing more than the declared bound must not silently exceed the memory reserved for this
///        event: the send fails instead.
TEST_F(SomeIpSkeletonEventFixture, SendFailsWhenTheSerializedSampleExceedsItsDeclaredBound)
{
    constexpr std::size_t kTightBound{2U};
    unit_->SetSampleSerialization(MakeSpec(MakeByteSwappingSerializer(), kTightBound));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0x12345678U};

    const auto result = SendByCopy(*unit_, value);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

/// \brief Exceeding the bound must not leak the slot either, otherwise the event runs out of slots.
TEST_F(SomeIpSkeletonEventFixture, ExceedingTheDeclaredBoundReturnsTheSlot)
{
    constexpr std::size_t kTightBound{2U};
    unit_->SetSampleSerialization(MakeSpec(MakeByteSwappingSerializer(), kTightBound));
    ASSERT_TRUE(unit_->PrepareOffer(MakeInitializeSampleCallback()).has_value());
    const TestSampleType value{0x12345678U};

    for (std::size_t i = 0U; i < (kNumberOfSlots + 1U); ++i)
    {
        EXPECT_FALSE(SendByCopy(*unit_, value).has_value());
    }

    EXPECT_TRUE(unit_->Allocate(SampleAllocateeGuard{}).has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::someip
