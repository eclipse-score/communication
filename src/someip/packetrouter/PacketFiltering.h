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

#ifndef LG_SOMEIP_PACKET_FILTERING_H
#define LG_SOMEIP_PACKET_FILTERING_H

#include <map>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <chrono>
#include <vector>
#include <memory>
#include <mutex>
#include <cassert>
#include <endpoint/EndpointBase.h>
#include <utils/time/TimerManager.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
#include <utils/statistics/SomeipPacketStatistics.h>
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

namespace lgsomeip {

class PacketFiltering {
    struct PeriodRecord {
        PeriodRecord(PacketFiltering* op, std::uint32_t id, std::chrono::milliseconds interval)
            : msg_id_(id), interval_(interval), activated_(false), timer_id_(0), total_dropped_msgs_(0),
              total_accepted_msgs_(0), parent_(op) {}
        virtual ~PeriodRecord() {
            deactivate();
        }

        void activate() {
            std::lock_guard<std::mutex> lk(mutex_);

            if (activated_)
                return;

            activated_ = true;
            dropped_msgs_ = 0;
            // PeriodRecord is activated since the first message is passed.
            total_accepted_msgs_++;
            blocked_msg_.clear();
            blocked_msg_endpoint_.reset();
            timer_id_ = parent_->timer_manager_->start_timer(
                interval_, std::bind(&PeriodRecord::on_timer_expired, this, std::placeholders::_1));
        }

        void deactivate() {
            std::lock_guard<std::mutex> lk(mutex_);
            deactivate_unlocked();
        }

        bool is_active() const {
            return activated_;
        }

        bool update(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length) {
            std::lock_guard<std::mutex> lk(mutex_);

            if (!activated_)
                return false;
            blocked_msg_endpoint_ = endpoint;
            if (!blocked_msg_.empty()) {
                // discard current blocked message
                total_dropped_msgs_++;
                dropped_msgs_++;

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                std::uint32_t message_id;
                get_byte_stream(&message_id, message);
                SomeipPacketStatistics::get_instance().increase_dropped_packet(
                    SomeipPacketStatistics::kIncoming, message_id,
                    SomeipPacketStatistics::kPacketRouterHostFilteredOut);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
            }
            blocked_msg_.assign(message, message + message_length);

            return true;
        }

        void on_timer_expired(TimerManager::TimerId timer_id) {
            std::unique_lock<std::mutex> lk(mutex_);
            LGSOMEIP_LOG_DEBUG << "PacketFiltering::on_timer_expired() " << to_string();

            if (!TimerManager::is_valid_timer(timer_id)) {
                LGSOMEIP_LOG_DEBUG << "PacketFiltering::on_timer_expired() Timer is not valid: " << timer_id;
                return;
            }
            if (timer_id_ != timer_id) {
                LGSOMEIP_LOG_WARN << "PeriodRecord::on_timer_expired() Got wrong timer id: expected = " << timer_id_
                                  << ", actual = " << timer_id;
                return;
            }

            if (!blocked_msg_.empty()) {
                // notify blocked message
                // Reserve addtional two bytes to be used later. (eg. in on_external_notification())
                std::vector<std::uint8_t> pass_msg(blocked_msg_.size() + 2);
                pass_msg = blocked_msg_;
                blocked_msg_.clear();
                std::shared_ptr<Endpoint> pass_msg_endpoint = std::move(blocked_msg_endpoint_);
                total_accepted_msgs_++;
                dropped_msgs_ = 0;
                lk.unlock();

                parent_->notify_message(pass_msg_endpoint, pass_msg.data(), pass_msg.size());
            } else {
                deactivate_unlocked();
            }
        }

        std::string to_string() const {
            std::ostringstream oss;
            oss << format_named_id("MessageID", msg_id_, 8) << ": ";
            oss << "interval = " << interval_.count() << " ms, ";
            oss << ", dropped_msgs_ = " << dropped_msgs_ << ", total_dropped_msgs = " << total_dropped_msgs_;
            oss << ", total_accepted_msgs_ = " << total_accepted_msgs_;

            return oss.str();
        }

        std::string to_string_condensed() const {
            std::ostringstream oss;
            oss << format_named_id("MessageID", msg_id_, 8);
            oss << "i" << interval_.count();
            oss << ",Drop:" << total_dropped_msgs_;
            oss << ",Accept:" << total_accepted_msgs_;

            return oss.str();
        }

        void deactivate_unlocked() {
            LGSOMEIP_LOG_DEBUG << "PeriodRecord::deactivate_unlocked() " << timer_id_;

            activated_ = false;
            blocked_msg_endpoint_.reset();
            // clear and return memory
            std::vector<uint8_t>().swap(blocked_msg_);
            if (timer_id_)
                parent_->timer_manager_->kill_timer(timer_id_);
            timer_id_ = 0;
        }

        std::mutex mutex_;
        const std::uint32_t msg_id_;
        std::chrono::milliseconds interval_;
        bool activated_;

        TimerManager::TimerId timer_id_;
        std::vector<uint8_t> blocked_msg_;
        std::shared_ptr<Endpoint> blocked_msg_endpoint_;
        std::uint32_t total_dropped_msgs_;
        std::uint32_t total_accepted_msgs_;
        std::uint32_t dropped_msgs_;

        PacketFiltering* parent_;
    };

public:
    enum FilterResult : uint8_t { kBlocked = 0, kPass = 1 };

public:
    PacketFiltering() : monitor_timer_(0) {
        timer_manager_ = &TimerManager::get();
    }
    virtual ~PacketFiltering() {}
    void set_filter_list(const std::map<std::uint32_t, std::uint16_t>& filters) {
        filter_list_ = filters;
    }

    void set_period_expire_func(
        std::function<void(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length)>
            callback) {
        period_expire_func_ = callback;
    }
    void notify_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length) {
        if (period_expire_func_) {
            period_expire_func_(endpoint, message, message_length);
        }
    }

    void enable_monitor(std::chrono::milliseconds milliseconds = std::chrono::milliseconds(100)) {
        if (monitor_timer_) {
            timer_manager_->kill_timer(monitor_timer_);
        }
        monitor_timer_ = timer_manager_->start_timer(std::chrono::milliseconds(milliseconds),
                                                     std::bind(&PacketFiltering::print_all_period, this));
    }
    void disable_monitor() {
        if (monitor_timer_) {
            timer_manager_->kill_timer(monitor_timer_);
        }
        monitor_timer_ = 0;
    }

    FilterResult filter_message(std::uint32_t id, std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                std::size_t message_length) {
        auto filter_config_iter{filter_list_.find(id)};
        if (filter_config_iter == filter_list_.end())
            return kPass;

        auto period_iter{periods_.find(id)};
        if (period_iter == periods_.end()) {
            auto it = periods_.insert(std::make_pair(
                id, std::make_shared<PeriodRecord>(this, id, std::chrono::milliseconds(filter_config_iter->second))));
            if (!it.second) {
                // TODO: fatal error
                LGSOMEIP_LOG_WARN << "Failed to create PeriodRecord for " << format_named_id("MessageID", id, 8);
                return kPass;
            }
            period_iter = it.first;
        }

        PeriodRecord* period = period_iter->second.get();
        if (!period->update(endpoint, message, message_length)) {
            period->activate();
            LGSOMEIP_LOG_DEBUG << "filter_message() passed a message for " << format_named_id("MessageID", id, 8);
            return kPass;
        } else {
            LGSOMEIP_LOG_DEBUG << "filter_message() blocked a message for " << format_named_id("MessageID", id, 8);
            return kBlocked;
        }
    }

    void print_all_period() {
        std::stringstream oss;
        uint16_t counter = 0;
        oss << "\n=PF_Records  (1st)=\n";
        for (const auto& iter : periods_) {
            oss << iter.second->to_string_condensed() << "\n";
            if ((++counter % 10) == 0) {
                std::string period_string = oss.str();
                LGSOMEIP_LOG_INFO << period_string;
                oss.str("");
                oss.clear();
                oss << "\n=PF_Records (cont)=\n";
            }
        }
        oss << "=====================";
        std::string period_string = oss.str();
        LGSOMEIP_LOG_INFO << period_string;
    }

protected:
private:
    std::map<std::uint32_t, std::shared_ptr<PeriodRecord>> periods_;
    std::map<std::uint32_t, std::uint16_t> filter_list_;
    TimerManager* timer_manager_;
    TimerManager::TimerId monitor_timer_;
    std::function<void(std::shared_ptr<Endpoint>, std::uint8_t*, std::size_t)> period_expire_func_;
};

} // namespace lgsomeip
#endif
