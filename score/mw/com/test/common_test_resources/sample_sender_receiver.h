/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SAMPLE_SENDER_RECEIVER_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SAMPLE_SENDER_RECEIVER_H

#include "score/concurrency/notification.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"
#include "score/os/utils/interprocess/interprocess_notification.h"
#include "score/result/result.h"

#include <score/stop_token.hpp>
#include <optional>

#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace score::mw::com::test
{

class EventSenderReceiver
{
  public:
    int RunAsSkeleton(const score::mw::com::InstanceSpecifier& instance_specifier,
                      const std::chrono::milliseconds cycle_time,
                      const std::size_t num_cycles,
                      const score::cpp::stop_token& stop_token);

    int RunAsSkeletonCheckEventSlots(const score::mw::com::InstanceSpecifier& instance_specifier,
                                     const std::uint16_t num_skeleton_slots,
                                     score::cpp::stop_source stop_source);

    int RunAsSkeletonCheckValuesCreatedFromConfig(
        const score::mw::com::InstanceSpecifier& instance_specifier,
        const std::string& shared_memory_path,
        score::os::InterprocessNotification& interprocess_notification_from_proxy,
        score::os::InterprocessNotification& interprocess_notification_to_proxy,
        score::cpp::stop_source stop_source);

    int RunAsSkeletonWaitForProxy(const score::mw::com::InstanceSpecifier& instance_specifier,
                                  score::os::InterprocessNotification& interprocess_notification,
                                  const score::cpp::stop_token& stop_token);

    /**
     * Basically the same as RunAsSkeleton(), but with 3 InterprocessNotification parameters that allow to inform
     * the other process (RunAsProxyWithNotificationExchange()) about the current status of the communication.
     * Those state notification objects are used to avoid races if services are offered/received multiple times.
     *    @param interprocess_notification_to_proxy Notify proxy that service has been offered
     *    @param interprocess_notification_from_proxy Proxy notifies skeleton that everything was received (so skeleton
     * can stop offering service)
     *    @param interprocess_notification_subscribed_from_proxy Proxy notifies skeleton that it has finished
     * subscribing (so skeleton can start sending samples)
     */
    int RunAsSkeletonWithNotificationExchange(
        const score::mw::com::InstanceSpecifier& instance_specifier,
        const std::chrono::milliseconds cycle_time,
        const std::size_t num_cycles,
        score::os::InterprocessNotification& interprocess_notification_from_proxy,
        score::os::InterprocessNotification& interprocess_notification_to_proxy,
        score::os::InterprocessNotification& interprocess_notification_subscribed_from_proxy,
        const score::cpp::stop_token& stop_token);

    template <typename ProxyType = score::mw::com::test::BigDataProxy,
              typename ProxyEventType = score::mw::com::impl::ProxyEvent<MapApiLanesStamped>>
    int RunAsProxy(const score::mw::com::InstanceSpecifier& instance_specifier,
                   const std::optional<std::chrono::milliseconds> cycle_time,
                   const std::size_t num_cycles,
                   const score::cpp::stop_token& stop_token,
                   bool try_writing_to_data_segment = false);

    /**
     * Setup a proxy and register a callback using SetReceiveHandler(). Returns successfully if the proxy stops
     * calling the callback after Unsubscribe is called.
     */
    int RunAsProxyReceiveHandlerOnly(const score::mw::com::InstanceSpecifier& instance_specifier,
                                     const score::cpp::stop_token& stop_token);

    /**
     * Counterpart of RunAsSkeletonWithNotificationExchange(): Similar to RunAsProxy() but with 3
     * InterProcessNotification parameters to exchange state information with the skeleton process.
     * @param interprocess_notification_from_skeleton Skeleton notifies about service offered
     * @param interprocess_notification_to_skeleton Inform skeleton that all data was received
     * @param interprocess_notification_subscribed_to_skeleton Inform skeleton that proxy has subscribed
     */
    int RunAsProxyWithNotificationExchange(
        const score::mw::com::InstanceSpecifier& instance_specifier,
        const std::chrono::milliseconds cycle_time,
        const std::size_t num_cycles,
        score::os::InterprocessNotification& interprocess_notification_from_skeleton,
        score::os::InterprocessNotification& interprocess_notification_to_skeleton,
        score::os::InterprocessNotification& interprocess_notification_subscribed_to_skeleton,
        const score::cpp::stop_token& stop_token);

    int RunAsProxyCheckEventSlots(const score::mw::com::InstanceSpecifier& instance_specifier,
                                  const std::uint16_t num_proxy_slots,
                                  score::cpp::stop_token stop_token);

    int RunAsProxyCheckValuesCreatedFromConfig(
        const score::mw::com::InstanceSpecifier& instance_specifier,
        const score::mw::com::impl::lola::ElementFqId map_api_lanes_element_fq_id_from_config,
        const score::mw::com::impl::lola::ElementFqId dummy_data_element_fq_id_from_config,
        const std::string& shared_memory_path,
        score::os::InterprocessNotification& interprocess_notification_from_skeleton,
        score::os::InterprocessNotification& interprocess_notification_to_skeleton,
        score::cpp::stop_token stop_token);

    int RunAsProxyCheckSubscribeHandler(const score::mw::com::InstanceSpecifier& instance_specifier,
                                        score::os::InterprocessNotification& interprocess_notification,
                                        score::cpp::stop_token stop_token);

  private:
    concurrency::Notification skeleton_finished_publishing_{};
    concurrency::Notification proxy_ready_to_receive_{};
    concurrency::Notification proxy_event_received_{};

    std::mutex event_sending_mutex_{};
    std::atomic<bool> event_published_{false};

    std::mutex map_lanes_mutex_{};
    std::vector<SamplePtr<MapApiLanesStamped>> map_lanes_list_{};
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_SAMPLE_SENDER_RECEIVER_H
