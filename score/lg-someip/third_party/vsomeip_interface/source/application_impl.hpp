// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_APPLICATION_IMPL_HPP
#define VSOMEIP_APPLICATION_IMPL_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <vsomeip/export.hpp>
#include <vsomeip/application.hpp>
#include <vsomeip/error.hpp>

#include <runtime/ApplicationManager.h>

namespace vsomeip {

class application_impl : public application, public std::enable_shared_from_this<application_impl> {
public:
    VSOMEIP_EXPORT application_impl(const std::string& _name);
    VSOMEIP_EXPORT ~application_impl();

    VSOMEIP_EXPORT void set_configuration(const std::shared_ptr<configuration> _configuration);

    VSOMEIP_EXPORT bool init();
    VSOMEIP_EXPORT void start();
    VSOMEIP_EXPORT void stop();

    // Provide services / events
    VSOMEIP_EXPORT void offer_service(service_t _service, instance_t _instance, major_version_t _major,
                                      minor_version_t _minor);

    VSOMEIP_EXPORT void stop_offer_service(service_t _service, instance_t _instance,
                                           major_version_t _major = DEFAULT_MAJOR,
                                           minor_version_t _minor = DEFAULT_MINOR);
    VSOMEIP_EXPORT void offer_event(service_t _service, instance_t _instance, event_t _event,
                                    const std::set<eventgroup_t>& _eventgroups, bool _is_field);
    VSOMEIP_EXPORT void offer_event(service_t _service, instance_t _instance, event_t _event,
                                    const std::set<eventgroup_t>& _eventgroups, bool _is_field,
                                    std::chrono::milliseconds _cycle, bool _change_resets_cycle,
                                    const epsilon_change_func_t& _epsilon_change_func);
    VSOMEIP_EXPORT void stop_offer_event(service_t _service, instance_t _instance, event_t _event);

    // Consume services / events
    VSOMEIP_EXPORT void request_service(service_t _service, instance_t _instance, major_version_t _major,
                                        minor_version_t _minor, bool _use_exclusive_proxy);
    VSOMEIP_EXPORT void release_service(service_t _service, instance_t _instance);

    VSOMEIP_EXPORT void request_event(service_t _service, instance_t _instance, event_t _event,
                                      const std::set<eventgroup_t>& _eventgroups, bool _is_field);
    VSOMEIP_EXPORT void release_event(service_t _service, instance_t _instance, event_t _event);

    VSOMEIP_EXPORT void subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                  major_version_t _major, subscription_type_e _subscription_type, event_t _event);

    VSOMEIP_EXPORT void unsubscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup);
    VSOMEIP_EXPORT void unsubscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event);

    VSOMEIP_EXPORT bool is_available(service_t _service, instance_t _instance, major_version_t _major = DEFAULT_MAJOR,
                                     minor_version_t _minor = DEFAULT_MINOR) const;

    VSOMEIP_EXPORT void send(std::shared_ptr<message> _message, bool _flush);

    VSOMEIP_EXPORT void notify(service_t _service, instance_t _instance, event_t _event,
                               std::shared_ptr<payload> _payload) const;
    VSOMEIP_EXPORT void notify(service_t _service, instance_t _instance, event_t _event,
                               std::shared_ptr<payload> _payload, bool _force) const;
    VSOMEIP_EXPORT void notify(service_t _service, instance_t _instance, event_t _event,
                               std::shared_ptr<payload> _payload, bool _force, bool _flush) const;

    VSOMEIP_EXPORT void notify_one(service_t _service, instance_t _instance, event_t _event,
                                   std::shared_ptr<payload> _payload, client_t _client) const;
    VSOMEIP_EXPORT void notify_one(service_t _service, instance_t _instance, event_t _event,
                                   std::shared_ptr<payload> _payload, client_t _client, bool _force) const;
    VSOMEIP_EXPORT void notify_one(service_t _service, instance_t _instance, event_t _event,
                                   std::shared_ptr<payload> _payload, client_t _client, bool _force, bool _flush) const;

    VSOMEIP_EXPORT void register_state_handler(state_handler_t _handler);
    VSOMEIP_EXPORT void unregister_state_handler();

    VSOMEIP_EXPORT void register_message_handler(service_t _service, instance_t _instance, method_t _method,
                                                 message_handler_t _handler, bool _is_provider = false);
    VSOMEIP_EXPORT void unregister_message_handler(service_t _service, instance_t _instance, method_t _method,
                                                   bool _is_provider = false);

    VSOMEIP_EXPORT void register_availability_handler(service_t _service, instance_t _instance,
                                                      availability_handler_t _handler,
                                                      major_version_t _major = DEFAULT_MAJOR,
                                                      minor_version_t _minor = DEFAULT_MINOR);
    VSOMEIP_EXPORT void unregister_availability_handler(service_t _service, instance_t _instance,
                                                        major_version_t _major = DEFAULT_MAJOR,
                                                        minor_version_t _minor = DEFAULT_MINOR);

    VSOMEIP_EXPORT void register_subscription_handler(service_t _service, instance_t _instance,
                                                      eventgroup_t _eventgroup, subscription_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_handler(service_t _service, instance_t _instance,
                                                        eventgroup_t _eventgroup);

    VSOMEIP_EXPORT void register_subscription_error_handler(service_t _service, instance_t _instance,
                                                            eventgroup_t _eventgroup, error_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_error_handler(service_t _service, instance_t _instance,
                                                              eventgroup_t _eventgroup);

    VSOMEIP_EXPORT bool is_routing() const;

    // routing_manager_host
    VSOMEIP_EXPORT const std::string& get_name() const;
    VSOMEIP_EXPORT client_t get_client() const;
    VSOMEIP_EXPORT std::shared_ptr<configuration> get_configuration() const;

    VSOMEIP_EXPORT void on_state(state_type_e _state);
    VSOMEIP_EXPORT void on_availability(service_t _service, instance_t _instance, bool _is_available,
                                        major_version_t _major, minor_version_t _minor);
    VSOMEIP_EXPORT void on_message(const std::shared_ptr<message>&& _message);
    VSOMEIP_EXPORT void on_error(error_code_e _error);
    VSOMEIP_EXPORT void on_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                        client_t _client, bool _subscribed, std::function<void(bool)> _accepted_cb);
    VSOMEIP_EXPORT void on_subscription_error(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              uint16_t _error);
    VSOMEIP_EXPORT void on_subscription_status(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                               event_t _event, uint16_t _error);
    VSOMEIP_EXPORT void register_subscription_status_handler(service_t _service, instance_t _instance,
                                                             eventgroup_t _eventgroup, event_t _event,
                                                             subscription_status_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_status_handler(service_t _service, instance_t _instance,
                                                               eventgroup_t _eventgroup, event_t _event);

    VSOMEIP_EXPORT bool are_available(available_t& _available, service_t _service = ANY_SERVICE,
                                      instance_t _instance = ANY_INSTANCE, major_version_t _major = ANY_MAJOR,
                                      minor_version_t _minor = ANY_MINOR) const;
    VSOMEIP_EXPORT void set_routing_state(routing_state_e _routing_state);

    VSOMEIP_EXPORT void clear_all_handler();

    VSOMEIP_EXPORT void register_subscription_status_handler(service_t _service, instance_t _instance,
                                                             eventgroup_t _eventgroup, event_t _event,
                                                             subscription_status_handler_t _handler,
                                                             bool _is_selective);

    VSOMEIP_EXPORT void get_offered_services_async(offer_type_e _offer_type, offered_services_handler_t _handler);

    VSOMEIP_EXPORT void on_offered_services_info(std::vector<std::pair<service_t, instance_t>>& _services);

    VSOMEIP_EXPORT void set_watchdog_handler(watchdog_handler_t _handler, std::chrono::seconds _interval);

    VSOMEIP_EXPORT void register_async_subscription_handler(service_t _service, instance_t _instance,
                                                            eventgroup_t _eventgroup,
                                                            async_subscription_handler_t _handler);

private:
    std::shared_ptr<lgsomeip::ApplicationManager> application_manager_;
    std::condition_variable condition_;
    std::mutex mutex_;
    volatile bool enabled_ = false;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP
