/********************************************************************************
 * Copyright (C) 2026 LG Electronics Inc.
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

#include <lgsomeip/LgsomeipApi.h>

#include <memory>
#include <stdexcept>
#include <utility>

#include <message/Message.h>
#include <runtime/ApplicationManager.h>

namespace lgsomeip {
namespace api {
namespace {

std::shared_ptr<MessageSOMEIP> to_internal_message(const Message& message) {
    auto result = std::make_shared<MessageSOMEIP>();
    const auto message_id = (static_cast<std::uint32_t>(message.service) << 16) | message.method;
    const auto request_id = (static_cast<std::uint32_t>(message.client) << 16) | message.session;

    result->set_message_id(message_id);
    result->set_instance_id(message.instance);
    result->set_request_id(request_id);
    result->set_interface_version(message.interface_version);
    result->set_message_type(static_cast<std::uint8_t>(message.type));
    result->set_return_code(static_cast<std::uint8_t>(message.return_code));
    auto payload = std::make_shared<MessagePayload>();
    payload->set_payload(message.payload);
    result->set_payload(std::move(payload));
    return result;
}

std::shared_ptr<Message> from_internal_message(const std::shared_ptr<MessageSOMEIP>& message) {
    if (!message) {
        return nullptr;
    }

    auto result = std::make_shared<Message>();
    const auto message_id = message->get_message_id();
    const auto request_id = message->get_request_id();
    result->service = static_cast<service_t>(message_id >> 16);
    result->method = static_cast<method_t>(message_id & 0xFFFF);
    result->instance = message->get_instance_id();
    result->client = static_cast<client_t>(request_id >> 16);
    result->session = static_cast<std::uint16_t>(request_id & 0xFFFF);
    result->interface_version = message->get_interface_version();
    result->type = static_cast<MessageType>(message->get_message_type());
    result->return_code = static_cast<ReturnCode>(message->get_return_code());
    if (const auto payload = message->get_payload_type()) {
        result->payload = payload->get_payload_vector();
    }
    return result;
}

class ApplicationImpl final : public Application {
public:
    ApplicationImpl(std::string name, std::string config_path)
        : manager_(std::make_shared<ApplicationManager>(std::move(name), std::move(config_path))) {}

    const std::string& name() const override { return manager_->get_application_name(); }
    client_t client_id() const override { return manager_->get_application_id(); }
    bool init() override {
        try {
            manager_->init();
            initialized_ = true;
            return true;
        } catch (...) {
            return false;
        }
    }
    void start() override {
        if (!initialized_) {
            throw std::logic_error("lgsomeip::api::Application::init must be called before start");
        }
        manager_->start();
    }
    void join() override {
        if (initialized_) {
            manager_->join();
        }
    }
    void stop() override { manager_->stop(); }

    void offer_service(service_t service, instance_t instance, major_version_t major, minor_version_t minor) override {
        manager_->offer_service(service, instance, major, minor);
    }
    void stop_offer_service(service_t service, instance_t instance, major_version_t major,
                            minor_version_t minor) override {
        manager_->stop_offer_service(service, instance, major, minor);
    }
    void find_service(service_t service, instance_t instance, major_version_t major, minor_version_t minor) override {
        manager_->find_service(service, instance, major, minor);
    }
    void request_service(service_t service, instance_t instance, major_version_t major,
                         minor_version_t minor) override {
        manager_->request_service(service, instance, major, minor);
    }
    void release_service(service_t service, instance_t instance) override {
        manager_->release_service(service, instance);
    }
    bool is_service_available(service_t service, instance_t instance, major_version_t major,
                              minor_version_t minor) const override {
        return manager_->is_service_available(service, instance, major, minor);
    }

    void offer_event(service_t service, instance_t instance, event_t event,
                     const std::set<eventgroup_t>& eventgroups, bool is_field) override {
        manager_->offer_event(service, instance, event, eventgroups, is_field);
    }
    void stop_offer_event(service_t service, instance_t instance, event_t event) override {
        manager_->stop_offer_event(service, instance, event);
    }
    void request_event(service_t service, instance_t instance, event_t event,
                       const std::set<eventgroup_t>& eventgroups) override {
        manager_->request_event(service, instance, event, eventgroups);
    }
    void release_event(service_t service, instance_t instance, event_t event) override {
        manager_->release_event(service, instance, event);
    }
    void subscribe(service_t service, instance_t instance, eventgroup_t eventgroup, major_version_t major,
                   event_t event_filter) override {
        manager_->subscribe(service, instance, eventgroup, major, event_filter);
    }
    void unsubscribe(service_t service, instance_t instance, eventgroup_t eventgroup) override {
        manager_->unsubscribe(service, instance, eventgroup);
    }

    void send(const Message& message) override { manager_->send(to_internal_message(message)); }
    void notify(service_t service, instance_t instance, event_t event, const Payload& payload, bool force,
                bool flush) override {
        auto internal_payload = std::make_shared<MessagePayload>();
        internal_payload->set_payload(payload);
        manager_->notify(service, instance, event, std::move(internal_payload), 0, force, flush);
    }

    void register_application_state_handler(ApplicationStateHandler handler) override {
        if (!handler) {
            manager_->unregister_application_state_handler();
            return;
        }
        manager_->register_application_state_handler(
            [handler = std::move(handler)](std::uint16_t state) { handler(state != 0); });
    }
    void register_message_handler(service_t service, instance_t instance, method_t method, MessageHandler handler,
                                  bool is_provider) override {
        if (!handler) {
            manager_->unregister_message_handler(service, instance, method, is_provider);
            return;
        }
        manager_->register_message_handler(service, instance, method,
                                           [handler = std::move(handler)](std::shared_ptr<MessageSOMEIP> message) {
                                               handler(from_internal_message(message));
                                           },
                                           is_provider);
    }
    void unregister_message_handler(service_t service, instance_t instance, method_t method,
                                    bool is_provider) override {
        manager_->unregister_message_handler(service, instance, method, is_provider);
    }
    void register_availability_handler(service_t service, instance_t instance, AvailabilityHandler handler,
                                       major_version_t major, minor_version_t minor) override {
        if (!handler) {
            manager_->unregister_availability_handler(service, instance, major, minor);
            return;
        }
        manager_->register_availability_handler(service, instance, std::move(handler), major, minor);
    }
    void unregister_availability_handler(service_t service, instance_t instance, major_version_t major,
                                         minor_version_t minor) override {
        manager_->unregister_availability_handler(service, instance, major, minor);
    }
    void register_subscription_handler(service_t service, instance_t instance, eventgroup_t eventgroup,
                                       SubscriptionHandler handler) override {
        if (!handler) {
            manager_->unregister_subscription_handler(service, instance, eventgroup);
            return;
        }
        manager_->register_subscription_handler(service, instance, eventgroup, std::move(handler));
    }
    void unregister_subscription_handler(service_t service, instance_t instance, eventgroup_t eventgroup) override {
        manager_->unregister_subscription_handler(service, instance, eventgroup);
    }

private:
    std::shared_ptr<ApplicationManager> manager_;
    bool initialized_{false};
};

class RuntimeImpl final : public Runtime {
public:
    std::shared_ptr<Application> create_application(const std::string& name,
                                                    const std::string& config_path) override {
        return std::make_shared<ApplicationImpl>(name, config_path);
    }
};

} // namespace

Application::~Application() = default;

Runtime& Runtime::instance() {
    static RuntimeImpl runtime;
    return runtime;
}

Runtime::~Runtime() = default;

} // namespace api
} // namespace lgsomeip