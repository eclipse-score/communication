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
#include "score/mw/com/impl/bindings/lola/service_data_control.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/memory/shared/new_delete_delegate_resource.h"

#include <score/span.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{42U};

// The exact numeric value of enforce_max_samples is irrelevant for the shm-layout, as it does not influence the
// capacity of any of the underlying (fixed-capacity) containers.
constexpr bool kEnforceMaxSamples{false};

/// \brief Constructs a real ServiceDataControl on the given resource and populates its event_controls_ with one
/// EventControl per entry of service_elements_size_info (mirroring what SkeletonMemoryManager does at runtime).
/// \return the number of bytes the given resource reports as allocated after construction.
std::size_t ConstructServiceDataControlAndGetAllocatedBytes(
    const std::vector<ServiceElementControlSizeInfo>& service_elements_size_info,
    memory::shared::ManagedMemoryResource& resource)
{
    auto& control = *resource.construct<ServiceDataControl>(service_elements_size_info.size(), resource);

    std::uint16_t element_id{0U};
    for (const auto& service_element : service_elements_size_info)
    {
        const ElementFqId element_fq_id{1U, element_id, 1U, ServiceElementType::EVENT};
        score::cpp::ignore = control.event_controls_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(element_fq_id),
            std::forward_as_tuple(static_cast<SlotIndexType>(service_element.number_of_slots),
                                  static_cast<EventControl::SubscriberCountType>(service_element.max_subscribers),
                                  kEnforceMaxSamples,
                                  resource));
        ++element_id;
    }

    return resource.GetUserAllocatedBytes();
}

using ServiceElementsSizeInfo = std::vector<ServiceElementControlSizeInfo>;

class ServiceDataControlShmSizeParameterizedTestFixture : public ::testing::TestWithParam<ServiceElementsSizeInfo>
{
};

TEST_P(ServiceDataControlShmSizeParameterizedTestFixture, CalculatedSizeMatchesActualAllocation)
{
    // Given the sizing information of some (possibly zero) service-elements (events/fields)
    const auto& service_elements_size_info = GetParam();

    // When calculating the required shm-size for a ServiceDataControl holding these service-elements
    const auto calculated_size = CalculateServiceDataControlShmSize(
        score::cpp::span<const ServiceElementControlSizeInfo>{service_elements_size_info});

    // Then the calculated size exactly matches the number of bytes actually allocated when constructing a real
    // ServiceDataControl (and its EventControls) with the very same sizing information.
    memory::shared::NewDeleteDelegateMemoryResource resource{kMemoryResourceId};
    const auto actual_allocated_bytes =
        ConstructServiceDataControlAndGetAllocatedBytes(service_elements_size_info, resource);

    EXPECT_EQ(calculated_size, actual_allocated_bytes);
}

INSTANTIATE_TEST_SUITE_P(
    ServiceDataControlShmSizeTests,
    ServiceDataControlShmSizeParameterizedTestFixture,
    ::testing::Values(
        // No service-elements at all (an empty span)
        ServiceElementsSizeInfo{},
        // A single service-element (event/field)
        ServiceElementsSizeInfo{ServiceElementControlSizeInfo{5U, 3U}},
        // Multiple service-elements (events/fields) with differing numbers of slots and maximum subscribers
        ServiceElementsSizeInfo{
            ServiceElementControlSizeInfo{2U, 1U},
            ServiceElementControlSizeInfo{7U, 4U},
            ServiceElementControlSizeInfo{1U, 10U},
        }));

TEST(ServiceDataControlShmSizeTest, IncreasingNumberOfSlotsOfAServiceElementIncreasesCalculatedSize)
{
    // Given two sizing infos for a single service-element that only differ in their number of slots
    const std::vector<ServiceElementControlSizeInfo> service_elements_with_fewer_slots{
        ServiceElementControlSizeInfo{2U, 3U}};
    const std::vector<ServiceElementControlSizeInfo> service_elements_with_more_slots{
        ServiceElementControlSizeInfo{20U, 3U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_with_fewer_slots = CalculateServiceDataControlShmSize(
        score::cpp::span<const ServiceElementControlSizeInfo>{service_elements_with_fewer_slots});
    const auto size_with_more_slots = CalculateServiceDataControlShmSize(
        score::cpp::span<const ServiceElementControlSizeInfo>{service_elements_with_more_slots});

    // Then the calculated size for the service-element with more slots is bigger, since each slot requires (amongst
    // others) additional space in the event's EventDataControl and in every subscriber's TransactionLog.
    EXPECT_GT(size_with_more_slots, size_with_fewer_slots);
}

TEST(ServiceDataControlShmSizeTest, IncreasingMaxSubscribersOfAServiceElementIncreasesCalculatedSize)
{
    // Given two sizing infos for a single service-element that only differ in their maximum number of subscribers
    const std::vector<ServiceElementControlSizeInfo> service_elements_with_fewer_subscribers{
        ServiceElementControlSizeInfo{2U, 1U}};
    const std::vector<ServiceElementControlSizeInfo> service_elements_with_more_subscribers{
        ServiceElementControlSizeInfo{2U, 20U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_with_fewer_subscribers = CalculateServiceDataControlShmSize(
        score::cpp::span<const ServiceElementControlSizeInfo>{service_elements_with_fewer_subscribers});
    const auto size_with_more_subscribers = CalculateServiceDataControlShmSize(
        score::cpp::span<const ServiceElementControlSizeInfo>{service_elements_with_more_subscribers});

    // Then the calculated size for the service-element with more subscribers is bigger, since every additional
    // subscriber requires its own TransactionLogNode (and the TransactionLog's slots array nested within it).
    EXPECT_GT(size_with_more_subscribers, size_with_fewer_subscribers);
}

TEST(ServiceDataControlShmSizeTest, AddingAnAdditionalServiceElementIncreasesCalculatedSize)
{
    // Given the sizing information of one service-element and, additionally, the very same sizing information for
    // two service-elements
    const std::vector<ServiceElementControlSizeInfo> single_service_element{ServiceElementControlSizeInfo{3U, 2U}};
    const std::vector<ServiceElementControlSizeInfo> two_service_elements{ServiceElementControlSizeInfo{3U, 2U},
                                                                          ServiceElementControlSizeInfo{3U, 2U}};

    // When calculating the required shm-size for both sizing infos
    const auto size_for_single_service_element = CalculateServiceDataControlShmSize(
        score::cpp::span<const ServiceElementControlSizeInfo>{single_service_element});
    const auto size_for_two_service_elements =
        CalculateServiceDataControlShmSize(score::cpp::span<const ServiceElementControlSizeInfo>{two_service_elements});

    // Then the calculated size for two service-elements is bigger than for a single one.
    EXPECT_GT(size_for_two_service_elements, size_for_single_service_element);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
