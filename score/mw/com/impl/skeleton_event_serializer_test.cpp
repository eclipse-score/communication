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
#include "score/mw/com/impl/skeleton_event.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/raw_wire_representation_fundamentals.h"
#include "score/mw/com/impl/sample_serialization_spec.h"
#include "score/mw/com/impl/serialization_sink.h"
#include "score/mw/com/impl/serialize_sample_callback.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint32_t;

using std::string_view_literals::operator""sv;

constexpr auto kEventName = "SerializerWiringEvent"sv;

/// \brief Minimal ISerializationSink collecting everything written into it.
class CollectingSink final : public ISerializationSink
{
  public:
    bool Write(const void* const data, const std::size_t size) noexcept override
    {
        const auto previous_size = bytes_.size();
        bytes_.resize(previous_size + size);
        std::memcpy(bytes_.data() + previous_size, data, size);
        return true;
    }

    const std::vector<std::byte>& GetBytes() const noexcept
    {
        return bytes_;
    }

  private:
    std::vector<std::byte> bytes_{};
};

/// \brief Binding which only records the serializer handed down by the strongly typed layer.
/// \details A hand-written fake (instead of mock_binding::SkeletonEvent) is used on purpose:
///          SetSerializeSampleCallback() has a default implementation on SkeletonEventBinding, so mocking it would
///          make every StrictMock<mock_binding::SkeletonEvent> in the existing tests fail on an uninteresting call.
class SerializerCapturingBinding final : public SkeletonEventBinding
{
  public:
    void SetSampleSerialization(std::optional<SampleSerializationSpec> spec) noexcept override
    {
        captured_spec_ = std::move(spec);
    }

    std::optional<SampleSerializationSpec>& GetCapturedSpec() noexcept
    {
        return captured_spec_;
    }

    Result<void> Send(SampleAllocateePtr<void>, std::optional<SendTraceCallback>) noexcept override
    {
        return {};
    }
    Result<SampleAllocateePtr<void>> Allocate(SampleAllocateeGuard) noexcept override
    {
        return SampleAllocateePtr<void>{};
    }
    Result<SamplePtr<void>> GetLatestSample(QualityType) override
    {
        return SamplePtr<void>{};
    }
    Result<void> PrepareOffer(const std::optional<InitializeSampleCallback>&) noexcept override
    {
        return {};
    }
    void PrepareStopOffer() noexcept override {}
    memory::DataTypeSizeInfo GetEventDataTypeSizeInfo() const override
    {
        return memory::DataTypeSizeInfo{sizeof(TestSampleType), alignof(TestSampleType)};
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kFake;
    }
    void SetSkeletonEventTracingData(tracing::SkeletonEventTracingData) noexcept override {}
    Result<void> Notify() noexcept override
    {
        return {};
    }
    Result<void> SetReceiveHandlerRegistrationChangedHandler(
        ReceiveHandlerRegistrationChangedCallback) noexcept override
    {
        return {};
    }
    Result<void> UnsetReceiveHandlerRegistrationChangedHandler() noexcept override
    {
        return {};
    }

  private:
    std::optional<SampleSerializationSpec> captured_spec_{};
};

class DummySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;
};

InstanceIdentifier MakeInstanceIdentifier()
{
    static const auto instance_specifier =
        InstanceSpecifier::Create(std::string{"abc/abc/SerializerWiringPort"}).value();
    static const ServiceInstanceDeployment deployment_info{make_ServiceIdentifierType("foo", 13, 37),
                                                           LolaServiceInstanceDeployment{LolaServiceInstanceId{23U}},
                                                           QualityType::kASIL_QM,
                                                           instance_specifier};
    static const ServiceTypeDeployment type_deployment{LolaServiceTypeDeployment{34U}};
    return make_InstanceIdentifier(deployment_info, type_deployment);
}

/// \brief The strongly typed layer is the only one which knows SampleDataType, so it has to hand a serializer down
///        to the type-erased binding. Without this, a binding needing a wire representation would silently fall back
///        to transmitting the raw object representation.
TEST(SkeletonEventSerializerWiringTest, ConstructionHandsASerializerToTheBinding)
{
    auto binding = std::make_unique<SerializerCapturingBinding>();
    auto& binding_ref = *binding;
    DummySkeleton skeleton{std::make_unique<mock_binding::Skeleton>(), MakeInstanceIdentifier()};

    const SkeletonEvent<TestSampleType> unit{skeleton, kEventName, std::move(binding)};

    EXPECT_TRUE(binding_ref.GetCapturedSpec().has_value());
}

TEST(SkeletonEventSerializerWiringTest, TheHandedOverSerializerWritesTheSampleRepresentation)
{
    auto binding = std::make_unique<SerializerCapturingBinding>();
    auto& binding_ref = *binding;
    DummySkeleton skeleton{std::make_unique<mock_binding::Skeleton>(), MakeInstanceIdentifier()};
    const SkeletonEvent<TestSampleType> unit{skeleton, kEventName, std::move(binding)};
    ASSERT_TRUE(binding_ref.GetCapturedSpec().has_value());

    const TestSampleType sample{0x0BADF00DU};
    CollectingSink sink{};
    binding_ref.GetCapturedSpec()->serialize(&sample, sink);

    ASSERT_EQ(sink.GetBytes().size(), sizeof(TestSampleType));
    TestSampleType round_tripped{};
    std::memcpy(&round_tripped, sink.GetBytes().data(), sizeof(TestSampleType));
    EXPECT_EQ(round_tripped, sample);
}

}  // namespace
}  // namespace score::mw::com::impl
