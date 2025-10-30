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
#ifndef SCORE_MW_COM_IPC_BRIDGE_SAMPLE_SENDER_RECEIVER_H
#define SCORE_MW_COM_IPC_BRIDGE_SAMPLE_SENDER_RECEIVER_H

#include "datatype.h"

#include <score/optional.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

namespace score::mw::com
{

class EventSenderReceiver
{
  public:
    int RunAsSkeleton(const score::mw::com::InstanceSpecifier& instance_specifier,
                      const std::chrono::milliseconds cycle_time,
                      const std::size_t num_cycles);

    template <typename ProxyType = score::mw::com::IpcBridgeProxy,
              typename ProxyEventType = score::mw::com::impl::ProxyEvent<MapApiLanesStamped>>
    int RunAsProxy(const score::mw::com::InstanceSpecifier& instance_specifier,
                   const score::cpp::optional<std::chrono::milliseconds> cycle_time,
                   const std::size_t num_cycles,
                   bool try_writing_to_data_segment = false,
                   bool check_sample_hash = true);

  private:
    std::mutex event_sending_mutex_{};
    std::atomic<bool> event_published_{false};

    std::mutex map_lanes_mutex_{};
    std::vector<SamplePtr<MapApiLanesStamped>> map_lanes_list_{};
};

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_IPC_BRIDGE_SAMPLE_SENDER_RECEIVER_H
