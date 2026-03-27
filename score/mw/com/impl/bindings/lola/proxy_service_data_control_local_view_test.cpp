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
#include "score/mw/com/impl/bindings/lola/service_data_control_local.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control_local.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/service_data_control.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr bool kEnforceMaxSamples{true};

class ServiceDataControlLocalFixture : public ::testing::Test
{
  public:
    ServiceDataControlLocalFixture()
    {
        score::cpp::ignore =
            service_data_control_.event_controls_.emplace(std::piecewise_construct,
                                                          std::forward_as_tuple(element_fq_id_0_),
                                                          std::forward_as_tuple(max_slots_0_,
                                                                                max_number_combined_subscribers_0_,
                                                                                kEnforceMaxSamples,
                                                                                memory_.getMemoryResourceProxy()));
        score::cpp::ignore =
            service_data_control_.event_controls_.emplace(std::piecewise_construct,
                                                          std::forward_as_tuple(element_fq_id_1_),
                                                          std::forward_as_tuple(max_slots_1_,
                                                                                max_number_combined_subscribers_1_,
                                                                                kEnforceMaxSamples,
                                                                                memory_.getMemoryResourceProxy()));
    }

    EventControlLocal& GetEventControlLocal(ServiceDataControlLocal& service_data_control_local,
                                            const ElementFqId element_fq_id)
    {
        auto event_control_local_it = service_data_control_local.event_controls_.find(element_fq_id);
        SCORE_LANGUAGE_FUTURECPP_ASSERT(event_control_local_it != service_data_control_local.event_controls_.end());
        return event_control_local_it->second;
    }

    EventControl& GetEventControl(ServiceDataControl& service_data_control, const ElementFqId element_fq_id)
    {
        auto event_control_it = service_data_control.event_controls_.find(element_fq_id);
        SCORE_LANGUAGE_FUTURECPP_ASSERT(event_control_it != service_data_control.event_controls_.end());
        return event_control_it->second;
    }

    ElementFqId element_fq_id_0_{10U, 20U, 30U, ServiceElementType::EVENT};
    ElementFqId element_fq_id_1_{11U, 21U, 31U, ServiceElementType::EVENT};

    SlotIndexType max_slots_0_{10U};
    SlotIndexType max_slots_1_{11U};

    LolaEventInstanceDeployment::SubscriberCountType max_number_combined_subscribers_0_{20U};
    LolaEventInstanceDeployment::SubscriberCountType max_number_combined_subscribers_1_{21U};

    FakeMemoryResource memory_{};
    ServiceDataControl service_data_control_{memory_.getMemoryResourceProxy()};
};

TEST_F(ServiceDataControlLocalFixture, ConstructsEventControlLocalPointingToEachEventControl)
{
    // When creating a ServiceDataControlLocal from a ServiceDataControl
    ServiceDataControlLocal service_data_control_local{service_data_control_};

    // Then the created ServiceDataControlLocal should contain EventDataControlLocals pointing to the EventDataControls
    // in the original ServiceDataControl. We check this by Allocating a slot in the EventDataControlLocal and checking
    // that the actual slot vector in EventDataControl is updated
    ASSERT_EQ(service_data_control_local.event_controls_.size(), 2);

    // Slots should initially be all 0
    auto& event_data_control_0{GetEventControl(service_data_control_, element_fq_id_0_).data_control};
    auto& event_data_control_1{GetEventControl(service_data_control_, element_fq_id_1_).data_control};
    std::all_of(
        event_data_control_0.state_slots_.begin(), event_data_control_0.state_slots_.end(), [](const auto& slot_value) {
            return slot_value == 0U;
        });
    std::all_of(
        event_data_control_1.state_slots_.begin(), event_data_control_1.state_slots_.end(), [](const auto& slot_value) {
            return slot_value == 0U;
        });

    auto& event_data_control_local_0 = GetEventControlLocal(service_data_control_local, element_fq_id_0_).data_control;
    auto& event_data_control_local_1 = GetEventControlLocal(service_data_control_local, element_fq_id_1_).data_control;

    // after allocating in the EventDataControlLocal
    score::cpp::ignore = event_data_control_local_0.AllocateNextSlot();
    score::cpp::ignore = event_data_control_local_1.AllocateNextSlot();

    // the EventDataControl should be updated
    EXPECT_EQ(event_data_control_0.state_slots_[0], std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(event_data_control_1.state_slots_[0], std::numeric_limits<std::uint32_t>::max());
}

TEST_F(ServiceDataControlLocalFixture, StoresReferenceToEventSubscriptionControlInServiceDataControl)
{
    // When creating a ServiceDataControlLocal from a ServiceDataControl
    ServiceDataControlLocal service_data_control_local{service_data_control_};

    // Then the EventSubscriptionControl reference for each EventControlLocal will point to the corresponding
    // EventSubscriptionControl in the ServiceDataControl
    ASSERT_EQ(service_data_control_local.event_controls_.size(), 2);
    auto& event_subscription_control_0{GetEventControl(service_data_control_, element_fq_id_0_).subscription_control};
    auto& event_subscription_control_1{GetEventControl(service_data_control_, element_fq_id_1_).subscription_control};

    auto event_subscription_control_local_0{
        GetEventControlLocal(service_data_control_local, element_fq_id_0_).subscription_control};
    auto event_subscription_control_local_1{
        GetEventControlLocal(service_data_control_local, element_fq_id_1_).subscription_control};

    EXPECT_EQ(&event_subscription_control_0, &event_subscription_control_local_0.get());
    EXPECT_EQ(&event_subscription_control_1, &event_subscription_control_local_1.get());
}

TEST_F(ServiceDataControlLocalFixture, StoresReferenceToApplicationIdPidMappingInServiceDataControl)
{
    // When creating a ServiceDataControlLocal from a ServiceDataControl
    ServiceDataControlLocal service_data_control_local{service_data_control_};

    // Then the ApplicationIdPidMapping reference will point to the ApplicationIdPidMapping in the ServiceDataControl
    EXPECT_EQ(&service_data_control_local.application_id_pid_mapping_,
              &service_data_control_.application_id_pid_mapping_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
