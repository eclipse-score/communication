// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <future>
#include <thread>
#include <iomanip>

#include <vsomeip/defines.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/plugins/application_plugin.hpp>
#include <vsomeip/plugins/pre_configuration_plugin.hpp>

#include "message_impl.hpp"
#include "payload_impl.hpp"
#include "application_impl.hpp"

#include <message/Message.h>
#include <runtime/ApplicationConstant.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

#define VSOMEIP_ENV_APPLICATION_NAME "VSOMEIP_APPLICATION_NAME"
#define VSOMEIP_ENV_CONFIGURATION "VSOMEIP_CONFIGURATION"
#define VSOMEIP_ENV_CONFIGURATION_MODULE "VSOMEIP_CONFIGURATION_MODULE"
#define VSOMEIP_ENV_MANDATORY_CONFIGURATION_FILES "VSOMEIP_MANDATORY_CONFIGURATION_FILES"
#define VSOMEIP_ENV_LOAD_PLUGINS "VSOMEIP_LOAD_PLUGINS"
#define VSOMEIP_ENV_CLIENTSIDELOGGING "VSOMEIP_CLIENTSIDELOGGING"
#define VSOMEIP_ENV_DEBUG_CONFIGURATION "VSOMEIP_DEBUG_CONFIGURATION"

namespace vsomeip {

application_impl::application_impl(const std::string& _name) {
    std::string its_name{_name};
    std::string its_config{lgsomeip::kConfigDefaultPath};

    if (its_name == "" || its_name == "no-name") {
        const char* env_name = getenv(VSOMEIP_ENV_APPLICATION_NAME);
        if (nullptr != env_name) {
            its_name = env_name;
        }
    }

    const char* env_config = getenv(VSOMEIP_ENV_CONFIGURATION);
    if (env_config != nullptr && strcmp(env_config, "") != 0) {
        its_config = env_config;
    }

    LGSOMEIP_LOG_DEBUG << "application_impl / name = " << its_name;
    LGSOMEIP_LOG_INFO << "application_impl / config = " << its_config;

    application_manager_ = std::make_shared<lgsomeip::ApplicationManager>(its_name, its_config);
}

application_impl::~application_impl() {
    runtime::get()->remove_application(get_name());
    application_manager_.reset();
}

void application_impl::set_configuration(const std::shared_ptr<configuration> _configuration) {}

bool application_impl::init() {
    application_manager_->init();
    return true;
}

void application_impl::start() {
    application_manager_->start();

    std::unique_lock<std::mutex> its_lock(mutex_);
    enabled_ = true;
    condition_.notify_all();
    its_lock.unlock();

    application_manager_->join();
}

void application_impl::stop() {
    application_manager_->stop();
}

void application_impl::offer_service(service_t _service, instance_t _instance, major_version_t _major,
                                     minor_version_t _minor) {
    std::unique_lock<std::mutex> its_lock(mutex_);
    if (!enabled_) {
        LGSOMEIP_LOG_DEBUG << "application_impl::offer_service / have to start app first. [" << lgsomeip::MSGID_FORMAT4(_service)
                           << ":" << lgsomeip::MSGID_FORMAT4(_instance) << "]";

        condition_.wait(its_lock);

        LGSOMEIP_LOG_DEBUG << "application_impl::offer_service / app started. [" << lgsomeip::MSGID_FORMAT4(_service) << ":"
                           << lgsomeip::MSGID_FORMAT4(_instance) << "]";
    }
    its_lock.unlock();

    application_manager_->offer_service(_service, _instance, _major, _minor);
}

void application_impl::stop_offer_service(service_t _service, instance_t _instance, major_version_t _major,
                                          minor_version_t _minor) {
    application_manager_->stop_offer_service(_service, _instance);
}

void application_impl::request_service(service_t _service, instance_t _instance, major_version_t _major,
                                       minor_version_t _minor, bool _use_exclusive_proxy) {
    // TODO : check parameter("_use_exclusive_proxy") usage
    application_manager_->request_service(_service, _instance, _major, _minor);
}

void application_impl::release_service(service_t _service, instance_t _instance) {
    application_manager_->release_service(_service, _instance);
}

void application_impl::subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                 major_version_t _major, subscription_type_e _subscription_type, event_t _event) {
    application_manager_->subscribe(_service, _instance, _eventgroup, _major, _event);
}

void application_impl::unsubscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    application_manager_->unsubscribe(_service, _instance, _eventgroup);
}

void application_impl::unsubscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event) {
    // TODO : check paramete("event") is need to use.
    application_manager_->unsubscribe(_service, _instance, _eventgroup);
}

bool application_impl::is_available(service_t _service, instance_t _instance, major_version_t _major,
                                    minor_version_t _minor) const {
    bool isAvailable = application_manager_->is_service_available(_service, _instance, _major == 0 ? ANY_MAJOR : _major,
                                                                 _minor == 0 ? ANY_MINOR : _minor);
    return isAvailable;
}

bool application_impl::are_available(available_t& _available, service_t _service, instance_t _instance,
                                     major_version_t _major, minor_version_t _minor) const {
    bool areAvailable = application_manager_->are_service_available(_available, _service, _instance, _major, _minor);
    return areAvailable;
}

void application_impl::send(std::shared_ptr<message> _message, bool _flush) {
    auto lsomeMsg = std::dynamic_pointer_cast<message_impl>(_message)->get_lgsomeip_message();

    application_manager_->send(lsomeMsg);
}

void application_impl::notify(service_t _service, instance_t _instance, event_t _event,
                              std::shared_ptr<payload> _payload) const {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        application_manager_->notify(_service, _instance, _event, payload->get_lgsomeip_payload());
    } catch (...) {
        LGSOMEIP_LOG_WARN << "application_impl::notify_1 / failed";
    }
}

void application_impl::notify(service_t _service, instance_t _instance, event_t _event,
                              std::shared_ptr<payload> _payload, bool _force) const {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        application_manager_->notify(_service, _instance, _event, payload->get_lgsomeip_payload(), 0, _force);
    } catch (...) {
        LGSOMEIP_LOG_WARN << "application_impl::notify_2 / failed";
    }
}

void application_impl::notify(service_t _service, instance_t _instance, event_t _event,
                              std::shared_ptr<payload> _payload, bool _force, bool _flush) const {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        application_manager_->notify(_service, _instance, _event, payload->get_lgsomeip_payload(), 0, _force, _flush);
    } catch (...) {
        LGSOMEIP_LOG_WARN << "application_impl::notify_3 / failed";
    }
}

void application_impl::notify_one(service_t _service, instance_t _instance, event_t _event,
                                  std::shared_ptr<payload> _payload, client_t _client) const {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        application_manager_->notify(_service, _instance, _event, payload->get_lgsomeip_payload(), _client);
    } catch (...) {
        LGSOMEIP_LOG_WARN << "application_impl::notify_one_1 / failed";
    }
}

void application_impl::notify_one(service_t _service, instance_t _instance, event_t _event,
                                  std::shared_ptr<payload> _payload, client_t _client, bool _force) const {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        application_manager_->notify(_service, _instance, _event, payload->get_lgsomeip_payload(), _client, _force);
    } catch (...) {
        LGSOMEIP_LOG_WARN << "application_impl::notify_one_2 / failed";
    }
}

void application_impl::notify_one(service_t _service, instance_t _instance, event_t _event,
                                  std::shared_ptr<payload> _payload, client_t _client, bool _force, bool _flush) const {
    try {
        auto payload = std::dynamic_pointer_cast<payload_impl>(_payload);
        application_manager_->notify(_service, _instance, _event, payload->get_lgsomeip_payload(), _client, _force,
                                    _flush);
    } catch (...) {
        LGSOMEIP_LOG_WARN << "application_impl::notify_one_3 / failed";
    }
}

void application_impl::register_state_handler(state_handler_t _handler) {
    application_manager_->register_application_state_handler([_handler](std::uint16_t state) {
        state_type_e vsomeipState = state_type_e::ST_REGISTERED;
        if (state == lgsomeip::SOMEIP_APPLICATION_DEREGISTERED) {
            vsomeipState = state_type_e::ST_DEREGISTERED;
        }

        if (_handler != nullptr)
            _handler(vsomeipState);
    });
}

void application_impl::unregister_state_handler() {
    application_manager_->unregister_application_state_handler();
}

void application_impl::register_availability_handler(service_t _service, instance_t _instance,
                                                     availability_handler_t _handler, major_version_t _major,
                                                     minor_version_t _minor) {
    if (_major == DEFAULT_MAJOR && _minor == DEFAULT_MINOR) {
        application_manager_->register_availability_handler(_service, _instance, _handler, ANY_MAJOR, ANY_MINOR);
    } else {
        application_manager_->register_availability_handler(_service, _instance, _handler, _major, _minor);
    }
}

void application_impl::unregister_availability_handler(service_t _service, instance_t _instance, major_version_t _major,
                                                       minor_version_t _minor) {
    if (_major == DEFAULT_MAJOR && _minor == DEFAULT_MINOR) {
        application_manager_->unregister_availability_handler(_service, _instance, ANY_MAJOR, ANY_MINOR);
    } else {
        application_manager_->unregister_availability_handler(_service, _instance, _major, _minor);
    }
}

void application_impl::on_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                       client_t _client, bool _subscribed, std::function<void(bool)> _accepted_cb) {
    // This callback method is not Used
}

void application_impl::register_subscription_handler(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                     subscription_handler_t _handler) {
    application_manager_->register_subscription_handler(_service, _instance, _eventgroup, _handler);
}

void application_impl::unregister_subscription_handler(service_t _service, instance_t _instance,
                                                       eventgroup_t _eventgroup) {
    // typedef std::function< bool (client_t, bool) > subscription_handler_t;
    application_manager_->unregister_subscription_handler(_service, _instance, _eventgroup);
}

void application_impl::on_subscription_status(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              event_t _event, uint16_t _error) {
    // This callback method is not Used
}

void application_impl::on_subscription_error(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                             uint16_t _error) {
    // This callback method is not Used
}

void application_impl::register_subscription_status_handler(service_t _service, instance_t _instance,
                                                            eventgroup_t _eventgroup, event_t _event,
                                                            subscription_status_handler_t _handler) {
    application_manager_->register_subscription_status_handler(_service, _instance, _eventgroup, _event, _handler,
                                                              false);
}

void application_impl::register_subscription_status_handler(service_t _service, instance_t _instance,
                                                            eventgroup_t _eventgroup, event_t _event,
                                                            subscription_status_handler_t _handler,
                                                            bool _is_selective) {
    application_manager_->register_subscription_status_handler(_service, _instance, _eventgroup, _event, _handler,
                                                              _is_selective);
}

void application_impl::unregister_subscription_status_handler(service_t _service, instance_t _instance,
                                                              eventgroup_t _eventgroup, event_t _event) {
    application_manager_->unregister_subscription_status_handler(_service, _instance, _eventgroup, _event);
}

void application_impl::register_subscription_error_handler(service_t _service, instance_t _instance,
                                                           eventgroup_t _eventgroup, error_handler_t _handler) {
    // typedef std::function< void (const uint16_t) > error_handler_t;

    // This method is not implemented
    throw std::runtime_error("application_impl::register_subscription_error_handler / not implemented");
}

void application_impl::unregister_subscription_error_handler(service_t _service, instance_t _instance,
                                                             eventgroup_t _eventgroup) {
    // This method is not implemented
    throw std::runtime_error("application_impl::unregister_subscription_error_handler / not implemented");
}

void application_impl::register_message_handler(service_t _service, instance_t _instance, method_t _method,
                                                message_handler_t _handler, bool _is_provider) {
    if (application_manager_ != nullptr) {
        application_manager_->register_message_handler(
            _service, _instance, _method,
            [_handler](std::shared_ptr<lgsomeip::Message> _lgsomeipMsg) {
                std::shared_ptr<message> vsomeipMsg = std::make_shared<message_impl>(_lgsomeipMsg);
                _handler(vsomeipMsg);
            },
            _is_provider);
    }
}

void application_impl::unregister_message_handler(service_t _service, instance_t _instance, method_t _method,
                                                  bool _is_provider) {
    application_manager_->unregister_message_handler(_service, _instance, _method, _is_provider);
}

void application_impl::offer_event(service_t _service, instance_t _instance, event_t _event,
                                   const std::set<eventgroup_t>& _eventgroups, bool _is_field) {
    application_manager_->offer_event(_service, _instance, _event, _eventgroups, _is_field);
}

void application_impl::offer_event(service_t _service, instance_t _instance, event_t _event,
                                   const std::set<eventgroup_t>& _eventgroups, bool _is_field,
                                   std::chrono::milliseconds _cycle, bool _change_resets_cycle,
                                   const epsilon_change_func_t& _epsilon_change_func) {
    if (_epsilon_change_func) {
        application_manager_->offer_event(_service, _instance, _event, _eventgroups, _is_field, _cycle.count(),
                                         [_epsilon_change_func](const std::shared_ptr<lgsomeip::Payload>& _lhs,
                                                                const std::shared_ptr<lgsomeip::Payload>& _rhs) {
                                             auto its_lhs = std::make_shared<payload_impl>(_lhs);
                                             auto its_rhs = std::make_shared<payload_impl>(_rhs);
                                             return _epsilon_change_func(its_lhs, its_rhs);
                                         });
    } else {
        application_manager_->offer_event(_service, _instance, _event, _eventgroups, _is_field, _cycle.count());
    }
}

void application_impl::stop_offer_event(service_t _service, instance_t _instance, event_t _event) {
    application_manager_->stop_offer_event(_service, _instance, _event);
}

void application_impl::request_event(service_t _service, instance_t _instance, event_t _event,
                                     const std::set<eventgroup_t>& _eventgroups, bool _is_field) {
    application_manager_->request_event(_service, _instance, _event, _eventgroups);
}

void application_impl::release_event(service_t _service, instance_t _instance, event_t _event) {
    application_manager_->release_event(_service, _instance, _event);
}

// Interface "routing_manager_host"
const std::string& application_impl::get_name() const {
    return application_manager_->get_application_name();
}

client_t application_impl::get_client() const {
    return application_manager_->get_application_id();
}

std::shared_ptr<configuration> application_impl::get_configuration() const {
    return nullptr;
}

void application_impl::on_state(state_type_e _state) {
    // This callback method is not Used
}

void application_impl::on_availability(service_t _service, instance_t _instance, bool _is_available,
                                       major_version_t _major, minor_version_t _minor) {
    // This callback method is not Used
}

void application_impl::on_message(const std::shared_ptr<message>&& _message) {
    // This callback method is not Used
}

void application_impl::on_error(error_code_e _error) {
    // This callback method is not Used
}

void application_impl::clear_all_handler() {
    application_manager_->clear_all_handler();
}

// void application_impl::shutdown() {
// }

bool application_impl::is_routing() const {
    return true;
}

void application_impl::set_routing_state(routing_state_e _routing_state) {
    throw std::runtime_error("application_impl::set_routing_state / not implemented");
}

void application_impl::get_offered_services_async(offer_type_e _offer_type, offered_services_handler_t _handler) {
    throw std::runtime_error("application_impl::get_offered_services_async / not implemented");
}

void application_impl::on_offered_services_info(std::vector<std::pair<service_t, instance_t>>& _services) {
    throw std::runtime_error("application_impl::on_offered_services_info / not implemented");
}

void application_impl::set_watchdog_handler(watchdog_handler_t _handler, std::chrono::seconds _interval) {
    throw std::runtime_error("application_impl::set_watchdog_handler / not implemented");
}

void application_impl::register_async_subscription_handler(service_t _service, instance_t _instance,
                                                           eventgroup_t _eventgroup,
                                                           async_subscription_handler_t _handler) {
    application_manager_->register_async_subscription_handler(_service, _instance, _eventgroup, _handler);
}

} // namespace vsomeip
