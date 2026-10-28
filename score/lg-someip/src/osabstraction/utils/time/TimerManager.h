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

#ifndef LG_SOMEIP_TIMER_MANAGER_H
#define LG_SOMEIP_TIMER_MANAGER_H

#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include <chrono>
#include <iostream>
#include <thread>
#include <queue>

namespace lgsomeip {
#define LG_SOMEIP_TO_MS_RESOLUTION(time_point) std::chrono::time_point_cast<std::chrono::milliseconds>(time_point)
#define LG_SOMEIP_INVALID_TIME LG_SOMEIP_TO_MS_RESOLUTION(std::chrono::steady_clock::time_point::max())
class TimerManager {
public:
    using TimerId = const void*;

protected:
    using TimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>;
    const std::chrono::milliseconds max_sleep_time_ = std::chrono::milliseconds(10000);

    class TimerInfo {
    public:
        TimerInfo(std::chrono::milliseconds interval, std::function<void(TimerId)> callback)
            : is_one_shot_(false), is_valid_(true), interval_(interval), func_(callback) {}

        TimerInfo(TimePoint now, std::chrono::milliseconds interval, std::function<void(TimerId)> callback,
                  bool one_shot)
            : is_one_shot_(one_shot), is_valid_(true), interval_(interval), func_(callback) {
            expire_time_ = now + interval_;
        }
        ~TimerInfo() {}

        bool is_valid() const {
            return is_valid_;
        }
        void invalidate() {
            is_valid_ = false;
        }
        void on_expired() {
            if (is_valid_ == true)
                func_(this);
        }

        void update_expire_time(TimePoint now) {
            expire_time_ = (is_one_shot_ ? LG_SOMEIP_INVALID_TIME : now + interval_);
        }

        TimePoint get_expire_time() const {
            return (is_valid_ ? expire_time_ : LG_SOMEIP_INVALID_TIME);
        }

    private:
        const bool is_one_shot_;
        bool is_valid_;
        std::chrono::milliseconds interval_;
        std::function<void(TimerId)> func_;
        TimePoint expire_time_;
    };

protected:
    TimerManager() {}

public:
    virtual ~TimerManager() {
        pending_timers_.clear();
    }

    static TimerManager& get() {
        static TimerManager instance;
        return instance;
    }

    TimerId start_one_shot_timer(std::chrono::milliseconds milliseconds, std::function<void(TimerId)> callback) {
        std::shared_ptr<TimerInfo> info = std::make_shared<TimerInfo>(
            LG_SOMEIP_TO_MS_RESOLUTION(std::chrono::steady_clock::now()), milliseconds, callback, true);

        std::unique_lock<std::mutex> lock(loop_mutex_);
        pending_timers_.push_back(info);
        lock.unlock();

        return info.get();
    }

    TimerId start_timer(std::chrono::milliseconds milliseconds, std::function<void(TimerId)> callback) {
        std::shared_ptr<TimerInfo> info = std::make_shared<TimerInfo>(milliseconds, callback);
        info->update_expire_time(LG_SOMEIP_TO_MS_RESOLUTION(std::chrono::steady_clock::now()));

        std::unique_lock<std::mutex> lock(loop_mutex_);
        pending_timers_.push_back(info);
        lock.unlock();

        return info.get();
    }

    void kill_timer(TimerId id) {
        TimerInfo* timer_pointer = static_cast<TimerInfo*>(const_cast<void*>(id));

        std::unique_lock<std::mutex> lock(loop_mutex_);
        for (auto it = pending_timers_.begin(); it != pending_timers_.end(); it++) {
            if (timer_pointer == (*it).get()) {
                (*it)->invalidate();
                pending_timers_.erase(it);
                break;
            }
        }
    }
    static bool is_valid_timer(TimerId id) {
        TimerInfo* timer_pointer = static_cast<TimerInfo*>(const_cast<void*>(id));
        return timer_pointer->is_valid();
    }

    TimePoint iterate_loop() {
        TimePoint till;

        std::unique_lock<std::mutex> lock(loop_mutex_);
        update_now();
        lock.unlock();

        check_timers();
        process_expired_timers();
        till = query_timeout();

        return till;
    }

protected:
    // HOLD loop_mutex_
    void update_now() {
        now_ = LG_SOMEIP_TO_MS_RESOLUTION(std::chrono::steady_clock::now());
    }

    void check_timers() {
        std::unique_lock<std::mutex> lock(loop_mutex_);

        for (auto& timer_info : pending_timers_) {
            TimePoint expiration_time = timer_info->get_expire_time();

            if (timer_info->is_valid() && expiration_time <= now_) {
                expired_timers_.push(timer_info);
                timer_info->update_expire_time(now_);
            }
        }
    }

    void process_expired_timers() {
        std::unique_lock<std::mutex> lock(loop_mutex_);
        while (!expired_timers_.empty()) {
            std::shared_ptr<TimerInfo> timer = expired_timers_.front();
            expired_timers_.pop();
            lock.unlock();
            timer->on_expired();
            lock.lock();
        }
    }

    TimePoint query_timeout() {
        std::unique_lock<std::mutex> lock(loop_mutex_);

        TimePoint minimum_expire_time = now_ + max_sleep_time_;

        for (auto it = pending_timers_.begin(); it != pending_timers_.end(); it++) {
            std::shared_ptr<TimerInfo> info = (*it);
            TimePoint expiration_time = info->get_expire_time();

            if (expiration_time != LG_SOMEIP_INVALID_TIME)
                minimum_expire_time = std::min(minimum_expire_time, expiration_time);
        }
        return minimum_expire_time;
    }

private:
    std::mutex loop_mutex_;
    std::list<std::shared_ptr<TimerInfo>> pending_timers_;
    std::queue<std::shared_ptr<TimerInfo>> expired_timers_;
    TimePoint now_;
};

} // namespace lgsomeip

#endif
