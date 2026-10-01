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

#include <thread>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

#include <exception/Exception.h>
#include <runtime/ApplicationManager.h>
#include <runtime/EventManager.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

namespace lgsomeip {

// -----------------------------------------------------------------------------
//  EventManager : Method Impl
// -----------------------------------------------------------------------------
EventManager::EventManager(ApplicationManager* host) : host_(host) {}

EventManager::~EventManager() {}

void EventManager::start() {
    if (running_ == false) {
        running_ = true;
        thread_ = std::make_shared<std::thread>(&EventManager::run, this);
        // For assigning thread name
        pthread_setname_np(thread_->native_handle(), "SomeipEventHdl");
    }
}

void EventManager::stop() {
    if (running_) {
        running_ = false;
    }
}

void EventManager::join() {
    std::mutex mutex;
    std::unique_lock<std::mutex> its_lock(mutex);

    do {
        if (thread_ != nullptr && thread_->joinable()) {
            thread_->join();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } while (running_);
}

void EventManager::run() {
    std::chrono::duration<int, std::milli> msec(base_interval_);
    while (running_) {
        {
            std::unique_lock<std::mutex> lock(thread_mutex_);
            for (auto event : offered_events_) {
                event->send(base_interval_);
            }
        }
        std::this_thread::sleep_for(msec);
    }
}

void EventManager::add_event(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                             std::uint8_t major_version, bool is_field, std::uint32_t cycle,
                             epsilon_change_func_t epsilon_change_function) {
    std::unique_lock<std::mutex> lock(thread_mutex_);

    // Find Registered Event;
    for (auto& event : offered_events_) {
        if (event->service_id_ == service_id && event->instance_id_ == instance_id && event->event_id_ == event_id) {
            LGSOMEIP_LOG_WARN << "EventManager::add_event / Event already registered "
                              << format_service_instance_event_id(service_id, instance_id, event_id);
            return;
        }
    }

    // Case1 : Not Registered. Do Register Event
    auto event = std::make_shared<OfferedEvent>(host_, service_id, instance_id, event_id, major_version, is_field,
                                                cycle, epsilon_change_function);
    if (event != nullptr) {
        LGSOMEIP_LOG_DEBUG << "EventManager::add_event / Register Offered Event "
                           << format_service_instance_event_id(service_id, instance_id, event_id);

        offered_events_.push_back(event);
    }
}

void EventManager::remove_event(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id) {
    std::unique_lock<std::mutex> lock(thread_mutex_);

    // Find Registered Event;
    for (auto iter = offered_events_.begin(); iter != offered_events_.end(); ++iter) {
        if ((*iter)->service_id_ == service_id && (*iter)->instance_id_ == instance_id &&
            (*iter)->event_id_ == event_id) {
            LGSOMEIP_LOG_DEBUG << "EventManager::remove_event / Deregister Offered Event "
                               << format_service_instance_event_id(service_id, instance_id, event_id);

            // remove the event
            offered_events_.erase(iter);
            return;
        }
    }
}

void EventManager::set_service_enabled(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                       bool state) {
    std::unique_lock<std::mutex> lock(thread_mutex_);

    // Set Payload in registered event;
    for (auto& event : offered_events_) {
        if (event->service_id_ == service_id && event->instance_id_ == instance_id) {
            event->set_service_enabled(service_id, instance_id, major_version, state);
        }
    }
}

void EventManager::notify(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                          std::shared_ptr<Payload> payload, std::uint16_t client, bool force, bool flush) {
    LGSOMEIP_LOG_DEBUG << "EventManager::notify "
                       << format_service_instance_event_id(service_id, instance_id, event_id);

    std::unique_lock<std::mutex> lock(thread_mutex_);

    // Set Payload in registered event;
    for (auto& event : offered_events_) {
        if (event->service_id_ == service_id && event->instance_id_ == instance_id && event->event_id_ == event_id) {
            event->set_payload(*payload, client, force);
            break;
        }
    }
}

void EventManager::notify_initial_event(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id) {
    std::unique_lock<std::mutex> lock(thread_mutex_);

    // Find Registered Event;
    for (auto& event : offered_events_) {
        if (event->service_id_ == service_id && event->instance_id_ == instance_id && event->event_id_ == event_id) {
            if (event->is_field() == true) {
                event->send_initial_event();
            } else {
                LGSOMEIP_LOG_DEBUG << "EventManager::notify_initial_event / "
                                   << format_service_instance_event_id(service_id, instance_id, event_id)
                                   << " is not Field. Initial Event will not be sent.";
            }
            break;
        }
    }
}

// -----------------------------------------------------------------------------
//  OfferedEvent : Method Impl
// -----------------------------------------------------------------------------
OfferedEvent::OfferedEvent(ApplicationManager* host, std::uint16_t service_id, std::uint16_t instance_id,
                           std::uint16_t event_id, std::uint8_t major_version, bool is_field, std::uint32_t cycle,
                           epsilon_change_func_t epsilon_change_function)
    : service_id_(service_id), instance_id_(instance_id), event_id_(event_id), major_version_(major_version),
      is_field_(is_field), update_cycle_(cycle), epsilon_change_function_(epsilon_change_function) {
    LGSOMEIP_LOG_DEBUG << "OfferedEvent::OfferedEvent / New event "
                       << format_service_instance_event_id(service_id, instance_id, event_id) << ", major["
                       << MSGID_FORMAT2(major_version) << "]";
    if (host != nullptr) {
        host_ = host;
        auto service_info = host_->get_configuration()->get_service_info(service_id, instance_id);
        if (service_info != nullptr) {
            init_event(service_info);
        } else {
            LGSOMEIP_LOG_DEBUG << "Configuration Error! / Service Info "
                               << format_service_instance_id(service_id, instance_id)
                               << " does not exist. Set Default value.";

            if (update_cycle_ > 0) {
                strategy_ = EventStrategy::CyclicUpdate;
            } else if (epsilon_change_function_) {
                strategy_ = EventStrategy::EpsilonChange;
            } else {
                strategy_ = EventStrategy::UpdateOnChange;
            }

            notification_ = MessageBuilder::create_notification_message(service_id_, event_id_);
            notification_->set_interface_version(major_version_);
        }
    }
}

void OfferedEvent::set_service_enabled(std::uint16_t service_id, std::uint16_t instance_id, std::uint8_t major_version,
                                       bool state) {
    std::unique_lock<std::mutex> lock(mutex_);

    LGSOMEIP_LOG_DEBUG << "OfferedEvent::set_service_enabled " << format_service_instance_id(service_id, instance_id)
                       << ", major[" << MSGID_FORMAT2(major_version) << "] : " << ((state) ? "enabled" : "disabled");

    if (service_id_ != service_id || instance_id_ != instance_id)
        return;

    service_enabled_ = state;
    major_version_ = major_version;
    if (notification_)
        notification_->set_interface_version(major_version_);
}

void OfferedEvent::init_event(ServiceInfo* service_info) {
    auto eginfo = service_info->get_events()->find(event_id_);
    if (eginfo == service_info->get_events()->end()) {
        LGSOMEIP_LOG_WARN << "OfferedEvent::initEvent / " << format_named_id("EventID", event_id_, 4)
                          << ", get_events is null";
        return;
    }

    auto eventinfo = service_info->get_event(event_id_);
    if (eventinfo == nullptr) {
        LGSOMEIP_LOG_WARN << "OfferedEvent::initEvent / " << format_named_id("EventID", event_id_, 4)
                          << ", get_event is null";
        return;
    }

    if (is_field_ != eventinfo->is_field()) {
        LGSOMEIP_LOG_DEBUG
            << "OfferedEvent::initEvent / Abandon existing is_field value. Using is_field value in json file.";
        is_field_ = eventinfo->is_field();
    }

    if (update_cycle_ != eventinfo->get_update_cycle()) {
        LGSOMEIP_LOG_DEBUG
            << "OfferedEvent::initEvent / Abandon existing Updatecycle value. Using Updatecycle value in json "
            << "file.";
        update_cycle_ = eventinfo->get_update_cycle();
    }

    LGSOMEIP_LOG_DEBUG << "OfferedEvent::initEvent "
                       << format_service_instance_event_id(this->service_id_, this->instance_id_, this->event_id_)
                       << " / isField = " << (this->is_field_ ? "true" : "false")
                       << " / Cycle = " << this->update_cycle_;

    if (update_cycle_ > 0) {
        strategy_ = EventStrategy::CyclicUpdate;
    } else if (epsilon_change_function_) {
        strategy_ = EventStrategy::EpsilonChange;
    } else {
        strategy_ = EventStrategy::UpdateOnChange;
    }

    notification_ = MessageBuilder::create_notification_message(service_id_, event_id_);
    notification_->set_interface_version(major_version_);
}

void OfferedEvent::set_payload(Payload& event_payload, std::uint16_t client, bool force, bool flush) {
    LGSOMEIP_LOG_DEBUG << "OfferedEvent::set_payload "
                       << format_service_instance_event_id(this->service_id_, this->instance_id_, this->event_id_);
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (notification_ == nullptr) {
            LGSOMEIP_LOG_WARN << "OfferedEvent::set_payload "
                              << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                  this->event_id_)
                              << " / notification_ is null!";
            return;
        }

        if (client > 0) { // for notify_one
            std::uint32_t sessionid = static_cast<std::uint32_t>(request_id_ & 0x0000ffff);
            std::uint32_t clientid = static_cast<std::uint32_t>(client) << 16;
            request_id_ = (clientid | sessionid);
        }

        auto payload = notification_->get_payload_type();

        switch (strategy_) {
        case EventStrategy::CyclicUpdate:
            if (payload->get_length() != event_payload.get_length() ||
                payload->get_payload_vector() != event_payload.get_payload_vector() || force) {
                notification_->set_payload(event_payload.get_payload(), event_payload.get_length());
                if (!initialized_) {
                    LGSOMEIP_LOG_DEBUG << "OfferedEvent::set_payload "
                                       << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                           this->event_id_)
                                       << " / CyclicUpdate success to set initial message!";
                    initialized_ = true;
                }
                lock.unlock();
            }
            if (flush) {
                send(update_cycle_);
            }
            break;
        case EventStrategy::UpdateOnChange:
            if (payload->get_length() != event_payload.get_length() ||
                payload->get_payload_vector() != event_payload.get_payload_vector() || force) {
                notification_->set_payload(event_payload.get_payload(), event_payload.get_length());
                if (!initialized_) {
                    LGSOMEIP_LOG_DEBUG << "OfferedEvent::set_payload "
                                       << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                           this->event_id_)
                                       << " / UpdateOnChange success to set initial message!";
                    initialized_ = true;
                }
                lock.unlock();
                send(0);
            } else {
                LGSOMEIP_LOG_INFO << "OfferedEvent::set_payload "
                                  << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                      this->event_id_)
                                  << " / does not send Event(Notifier) due to the same payload as before!";
            }
            break;
        case EventStrategy::EpsilonChange:
            if (epsilon_change_function_(payload, std::make_shared<Payload>(event_payload))) {
                notification_->set_payload(event_payload.get_payload(), event_payload.get_length());
                if (!initialized_) {
                    LGSOMEIP_LOG_DEBUG << "OfferedEvent::set_payload "
                                       << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                           this->event_id_)
                                       << " / EpsilonChange success to set initial message!";
                    initialized_ = true;
                }
                lock.unlock();
                send(0);
            }
            break;
        default:
            LGSOMEIP_LOG_ERROR << "OfferEvent::set_payload / EventStrategy is not valid";
            break;
        }
    }
}

void OfferedEvent::send(std::uint32_t interval) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!service_enabled_) {
        LGSOMEIP_LOG_DEBUG << "OfferedEvent::send / Service is not enabled. "
                           << format_service_instance_event_id(this->service_id_, this->instance_id_, this->event_id_);
        return;
    }

    if (initialized_ == false) {
        if (!print_not_initialized_message_) {
            LGSOMEIP_LOG_DEBUG << "OfferedEvent::send / not initialized "
                               << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                   this->event_id_);
            print_not_initialized_message_ = true;
        }
        return;
    }

    // Case 1: Update on Change / Epsilon Change
    if ((strategy_ == EventStrategy::UpdateOnChange || strategy_ == EventStrategy::EpsilonChange) && interval == 0) {
        LGSOMEIP_LOG_DEBUG << "OfferedEvent::send (UpdateOnChange or EpsilonChange) / "
                           << format_service_instance_event_id(this->service_id_, this->instance_id_, this->event_id_);
        send_notification();
    }

    // Case 2 : Cyclic Update
    if (strategy_ == EventStrategy::CyclicUpdate && interval > 0) {
        remaining_update_ = (remaining_update_ > interval) ? remaining_update_ - interval : 0;
        if (remaining_update_ == 0) {
            // Send Message
            LGSOMEIP_LOG_DEBUG << "OfferedEvent::send (CyclicUpdate) / "
                               << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                   this->event_id_);
            send_notification();
            remaining_update_ = update_cycle_;
        }
    }
}

void OfferedEvent::send_initial_event() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (initialized_ == false) {
        if (is_field_ == true) {
            LGSOMEIP_LOG_WARN << "OfferedEvent::send_initial_event "
                              << format_service_instance_event_id(this->service_id_, this->instance_id_,
                                                                  this->event_id_)
                              << " / not initialized";
        }
        return;
    }

    LGSOMEIP_LOG_DEBUG << "OfferedEvent::send_initial_event "
                       << format_service_instance_event_id(this->service_id_, this->instance_id_, this->event_id_);

    if (is_field_ == true) {
        // Send Initial Event Message
        send_notification();
    }
}

void OfferedEvent::send_notification() {
    if (host_ != nullptr) {
        request_id_ = (request_id_ % 0x0000FFFF) + 1;
        notification_->set_request_id(request_id_);
        notification_->set_instance_id(instance_id_);
        host_->send(notification_);
    }
}

} // namespace lgsomeip
