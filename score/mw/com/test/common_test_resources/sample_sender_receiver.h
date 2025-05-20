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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SAMPLE_SENDER_RECEIVER_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SAMPLE_SENDER_RECEIVER_H

#include "score/concurrency/notification.h"
#include "score/os/utils/interprocess/interprocess_notification.h"
#include "score/result/result.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"

#include <amp_optional.hpp>
#include <amp_stop_token.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace bmw
{
  namespace mw
  {
    namespace com
    {

      namespace test
      {

        class EventSenderReceiver
        {
        public:
          int RunAsSkeleton(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                            const std::chrono::milliseconds cycle_time,
                            const std::size_t num_cycles,
                            const amp::stop_token &stop_token);

          int RunAsSkeletonCheckEventSlots(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                                           const std::uint16_t num_skeleton_slots,
                                           amp::stop_source stop_source);

          int RunAsSkeletonCheckValuesCreatedFromConfig(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                                                        const std::string &shared_memory_path,
                                                        bmw::os::InterprocessNotification &interprocess_notification,
                                                        amp::stop_source stop_source);

          int RunAsSkeletonWaitForProxy(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                                        bmw::os::InterprocessNotification &interprocess_notification,
                                        const amp::stop_token &stop_token);

          template <typename ProxyType = bmw::mw::com::test::BigDataProxy,
                    typename ProxyEventType = bmw::mw::com::impl::ProxyEvent<MapApiLanesStamped>>
          int RunAsProxy(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                         const amp::optional<std::chrono::milliseconds> cycle_time,
                         const std::size_t num_cycles,
                         const amp::stop_token &stop_token,
                         bool try_writing_to_data_segment = false);

          /**
           * Setup a proxy and register a callback using SetReceiveHandler(). Returns successfully if the proxy stops
           * calling the callback after Unsubscribe is called.
           */
          int RunAsProxyReceiveHandlerOnly(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                                           const amp::stop_token &stop_token);

          int RunAsProxyCheckEventSlots(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                                        const std::uint16_t num_proxy_slots,
                                        amp::stop_token stop_token);

          int RunAsProxyCheckValuesCreatedFromConfig(
              const bmw::mw::com::InstanceSpecifier &instance_specifier,
              const bmw::mw::com::impl::lola::ElementFqId map_api_lanes_element_fq_id_from_config,
              const bmw::mw::com::impl::lola::ElementFqId dummy_data_element_fq_id_from_config,
              const std::string &shared_memory_path,
              bmw::os::InterprocessNotification &interprocess_notification,
              amp::stop_token stop_token);

          int RunAsProxyCheckSubscribeHandler(const bmw::mw::com::InstanceSpecifier &instance_specifier,
                                              bmw::os::InterprocessNotification &interprocess_notification,
                                              amp::stop_token stop_token);

        private:
          concurrency::Notification skeleton_finished_publishing_{};
          concurrency::Notification proxy_ready_to_receive_{};
          concurrency::Notification proxy_event_received_{};

          std::mutex event_sending_mutex_{};
          std::atomic<bool> event_published_{false};

          std::mutex map_lanes_mutex_{};
          std::vector<SamplePtr<MapApiLanesStamped>> map_lanes_list_{};
        };

      } // namespace test

    } // namespace com
  } // namespace mw
} // namespace bmw

#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SAMPLE_SENDER_RECEIVER_H
