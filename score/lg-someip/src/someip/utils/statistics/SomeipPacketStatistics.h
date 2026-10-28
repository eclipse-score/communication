/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_SOMEIP_PACKET_STATISTICS_H
#define LG_SOMEIP_SOMEIP_PACKET_STATISTICS_H

#include <cstdint>
#include <map>
#include <mutex>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>
#include <utils/time/TimerManager.h>

namespace lgsomeip {

constexpr std::uint16_t kStatisticsPrintInterval = 15000;
constexpr std::uint16_t kStatisticsPrintLogMaxLength = 350;

class SomeipPacketStatistics {
public:
    using MessageId = std::uint32_t;
    using ServiceId = std::uint16_t;

    enum Direction : std::uint8_t { kOutgoing = 0, kIncoming = 1 };

    enum StatisticsType : std::uint8_t {
        // Type for receiving and delivering SOME/IP
        kSomeipReceived = 0,
        kSomeipDelivered = 1,

        // Type for receving and delivering SOME/IP-SD
        kSomeipSdFindServiceReceived = 2,
        kSomeipSdOfferServiceReceived = 3,
        kSomeipSdSubscribeReceived = 4,
        kSomeipSdSubscribeAckReceived = 5,

        kSomeipSdTriggerDumpSomeipServices = 10,
        kSomeipSdUnknownSdPort = 11,
        kSomeipSdInvalidMessage = 12,

        kSomeipSdFindServiceIamDenied = 20,
        kSomeipSdFindServiceTtlZero = 21,
        kSomeipSdFindServiceNoServiceInfo = 22,
        kSomeipSdFindServiceNoProviderInside = 23,
        kSomeipSdFindServiceNoMatchedProvider = 24,

        kSomeipSdOfferServiceIamDenied = 30,
        kSomeipSdOfferServiceHandleStopOfferNoServiceInfo = 31,
        kSomeipSdOfferServiceHandleNewOfferNoServiceInfo = 32,
        kSomeipSdOfferServiceHandleNewOfferFailed = 33,
        kSomeipSdOfferServiceNoEndpoint = 34,
        kSomeipSdOfferServiceNoConfig = 35,
        kSomeipSdOfferServiceInvalidConfig = 36,

        kSomeipSdSubscribeNoServiceInfo = 40,
        kSomeipSdSubscribeCheckError = 41,
        kSomeipSdSubscribeNoAddrOption = 42,
        kSomeipSdSubscribeNoProviderInside = 43,
        kSomeipSdSubscribeAckNoServiceInfo = 44,
        kSomeipSdSubscribeAckNoEventConfig = 45,
        kSomeipSdSubscribeAckNoConfig = 46,

        // Type for dropped SOME/IP packet
        kPacketRouterHostNoConnection = 50,
        kPacketRouterHostNoServiceInfo = 51,
        kPacketRouterHostNoSubscriptionInfo = 52,
        kPacketRouterHostNoTargetClient = 53,
        kPacketRouterHostNoSubscription = 54,
        kPacketRouterHostOnExternalMessageError = 55,
        kPacketRouterHostInvalidReturnCode = 56,
        kPacketRouterHostNoRequestForResponse = 57,
        kPacketRouterHostNoEndPoint = 58,
        kPacketRouterHostFilteredOut = 59,

        kApplicationManagerError = 60,
        kApplicationManagerException = 61,
        kApplicationManagerWrongRequestId = 62,
        kPacketRouterProxyEndpointNullptr = 63,

        // Type for SOME/IP binding callbacks
        kBindingEventReceived = 80,
        kBindingEventCallAppCallback = 81,
        kBindingEventPushQueue = 82,
        kBindingEventValidationError = 85,
        kBindingEventDeserializationError = 86,

        kBindingMethodReceived = 90,
        kBindingMethodDelivered = 91,
        kBindingMethodErrorDelivered = 95
    };

    SomeipPacketStatistics(void) {
        print_interval_timer_ =
            TimerManager::get().start_timer(std::chrono::milliseconds(kStatisticsPrintInterval),
                                            std::bind(&SomeipPacketStatistics::print_statistics, this));

        if (print_interval_timer_) {
            LGSOMEIP_LOG_INFO << "SomeipPacketStatistics is enabled and started";
        } else {
            LGSOMEIP_LOG_ERROR
                << "SomeipPacketStatistics is enabled but cannot start due to failing creating a timer!!!";
        }
    }

    ~SomeipPacketStatistics(void) {
        if (print_interval_timer_) {
            TimerManager::get().kill_timer(print_interval_timer_);

            LGSOMEIP_LOG_INFO << "The timer for SomeipPacketStatistics stopped";
        }

        LGSOMEIP_LOG_INFO << "SomeipPacketStatistics ended";
    }

    static SomeipPacketStatistics& get_instance(void) {
        static SomeipPacketStatistics instance; // Singleton instance

        return instance;
    }

    void increase_received_packet(const Direction direction, const MessageId message_id) {
        std::lock_guard<std::mutex> guard(statistics_mutex_);
        statistics_[direction][message_id][kSomeipReceived]++;
    }

    void increase_delivered_packet(const Direction direction, const MessageId message_id) {
        std::lock_guard<std::mutex> guard(statistics_mutex_);
        statistics_[direction][message_id][kSomeipDelivered]++;
    }

    void increase_sd_type_packet(const Direction direction, const ServiceId service_id,
                                 const StatisticsType statistics_type) {
        std::lock_guard<std::mutex> guard(statistics_mutex_);
        std::uint32_t message_id = (static_cast<std::uint32_t>(service_id) << 16);
        statistics_[direction][message_id][statistics_type]++;
    }

    void increase_dropped_packet(const Direction direction, const MessageId message_id,
                                 const StatisticsType drop_reason) {
        std::lock_guard<std::mutex> guard(statistics_mutex_);
        statistics_[direction][message_id][drop_reason]++;
    }

    void print_statistics(void) {
        std::stringstream oss;
        std::map<Direction, std::map<MessageId, std::map<StatisticsType, std::uint32_t>>> statistics_snapshot;

        {
            std::lock_guard<std::mutex> guard(statistics_mutex_);
            statistics_snapshot = statistics_;
        }

        oss << "\n=Statistics  (1st)=\n";
        for (const auto& first_iter : statistics_snapshot) {
            for (const auto& second_iter : first_iter.second) {
                oss << "{Dir:" << first_iter.first << ",[" << MSGID_FORMAT8(second_iter.first) << "][";
                for (const auto& third_iter : second_iter.second) {
                    oss << third_iter.first << ":" << third_iter.second << ",";
                }
                oss << "]}";

                if (oss.tellp() > kStatisticsPrintLogMaxLength) {
                    std::string period_string = oss.str();
                    LGSOMEIP_LOG_INFO << period_string;
                    oss.str("");
                    oss.clear();
                    oss << "\n=Statistics (cont)=\n";
                }
            }
        }
        oss << "=====================";
        std::string period_string = oss.str();
        LGSOMEIP_LOG_INFO << period_string;
    }

private:
    std::map<Direction, std::map<MessageId, std::map<StatisticsType, std::uint32_t>>> statistics_;
    std::mutex statistics_mutex_;
    TimerManager::TimerId print_interval_timer_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_SOMEIP_PACKET_STATISTICS_H
