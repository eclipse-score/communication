/********************************************************************************
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
 *
 ********************************************************************************/

// Integration tests of the public lg-someip API (include/lgsomeip/LgsomeipApi.h).
// The applications use the public API only. The routing daemon is the internal
// ServiceManager, because the public API does not expose a daemon.

#include <lgsomeip/LgsomeipApi.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <runtime/ServiceManager.h>

namespace
{

namespace api = lgsomeip::api;

constexpr auto kTimeout = std::chrono::seconds(10);
constexpr auto kQuietPeriod = std::chrono::seconds(1);
constexpr auto kUnsubscribeSettleTime = std::chrono::milliseconds(500);

constexpr api::service_t kService = 0x1001;
constexpr api::instance_t kInstance = 0x0001;
constexpr api::method_t kMethod = 0x0001;
constexpr api::major_version_t kMajor = 0x01;
constexpr api::minor_version_t kMinor = 0x00000000;
constexpr api::eventgroup_t kEventGroup = 0x4455;
constexpr api::event_t kEvent = 0x8777;
constexpr api::event_t kField = 0x8778;

constexpr api::client_t kClientClientId = 0x0121;

// Routing daemon.
class ApiTestDaemon
{
  public:
    explicit ApiTestDaemon(const std::string& config_path)
        : service_manager_(std::make_shared<lgsomeip::ServiceManager>("daemon", config_path))
    {
        service_manager_->init();
        service_manager_->start();
    }

    ~ApiTestDaemon()
    {
        service_manager_->stop();
    }

  private:
    std::shared_ptr<lgsomeip::ServiceManager> service_manager_;
};

// Application that is created through the public API.
class ApiApp
{
  public:
    ApiApp(const std::string& name, const std::string& config_path)
        : app_(api::Runtime::instance().create_application(name, config_path))
    {
    }

    ~ApiApp()
    {
        stop();
    }

    ApiApp(const ApiApp&) = delete;
    ApiApp& operator=(const ApiApp&) = delete;

    api::Application* operator->()
    {
        return app_.get();
    }

    api::Application* get()
    {
        return app_.get();
    }

    // Initializes the application and tracks its registration with the daemon.
    bool init()
    {
        if (!app_->init())
        {
            return false;
        }
        app_->register_application_state_handler([this](bool registered) {
            std::lock_guard<std::mutex> lock(mutex_);
            connected_ = registered;
            condition_.notify_all();
        });
        return true;
    }

    void start()
    {
        app_->start();
        started_ = true;
    }

    void stop()
    {
        if (app_ && started_)
        {
            app_->stop();
            app_->join();
        }
        started_ = false;
        app_.reset();
    }

    bool wait_connected(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, timeout, [this] {
            return connected_;
        });
    }

  private:
    std::shared_ptr<api::Application> app_;
    bool started_{false};
    std::mutex mutex_;
    std::condition_variable condition_;
    bool connected_{false};
};

// Collects the messages that a handler receives.
class Inbox
{
  public:
    api::MessageHandler handler()
    {
        return [this](const std::shared_ptr<api::Message>& message) {
            std::lock_guard<std::mutex> lock(mutex_);
            messages_.push_back(message);
            condition_.notify_all();
        };
    }

    bool wait_for_count(std::size_t count, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, timeout, [this, count] {
            return messages_.size() >= count;
        });
    }

    std::vector<std::shared_ptr<api::Message>> messages() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<std::shared_ptr<api::Message>> messages_;
};

// Tracks the availability of one service instance.
class Availability
{
  public:
    api::AvailabilityHandler handler()
    {
        return [this](api::service_t, api::instance_t, bool available) {
            std::lock_guard<std::mutex> lock(mutex_);
            available_ = available;
            ++updates_;
            condition_.notify_all();
        };
    }

    bool wait(bool expected, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, timeout, [this, expected] {
            return available_ == expected;
        });
    }

    bool wait_for_update(std::size_t previous_updates, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, timeout, [this, previous_updates] {
            return updates_ > previous_updates;
        });
    }

    bool available() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_;
    }

    std::size_t updates() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return updates_;
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool available_{false};
    std::size_t updates_{0};
};

// Records the subscription requests that reach the provider.
class SubscriptionLog
{
  public:
    api::SubscriptionHandler handler(bool accept)
    {
        return [this, accept](api::client_t client, bool subscribed) {
            std::lock_guard<std::mutex> lock(mutex_);
            entries_.emplace_back(client, subscribed);
            condition_.notify_all();
            return accept;
        };
    }

    bool wait_for(api::client_t client, bool subscribed, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, timeout, [this, client, subscribed] {
            for (const auto& entry : entries_)
            {
                if (entry.first == client && entry.second == subscribed)
                {
                    return true;
                }
            }
            return false;
        });
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<std::pair<api::client_t, bool>> entries_;
};

// Provider-side handler that records a request and answers it with the same payload.
api::MessageHandler echo_handler(api::Application* application, Inbox& requests)
{
    auto record = requests.handler();
    return [application, record](const std::shared_ptr<api::Message>& request) {
        record(request);
        if (request->type != api::MessageType::Request)
        {
            return;
        }
        api::Message response = *request;
        response.type = api::MessageType::Response;
        response.return_code = api::ReturnCode::Ok;
        application->send(response);
    };
}

api::Message make_request(const api::Payload& payload, api::major_version_t major = kMajor)
{
    api::Message request;
    request.service = kService;
    request.instance = kInstance;
    request.method = kMethod;
    request.interface_version = major;
    request.type = api::MessageType::Request;
    request.payload = payload;
    return request;
}

class LgsomeipApiRuntimeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config_path_ = std::string(LGSOMEIP_TEST_CONFIG_DIR) + "/someip_config.json";
        daemon_ = std::make_unique<ApiTestDaemon>(config_path_);
    }

    void TearDown() override
    {
        daemon_.reset();
    }

    std::string config_path_;
    std::unique_ptr<ApiTestDaemon> daemon_;
};

TEST_F(LgsomeipApiRuntimeTest, application_registers_with_the_daemon)
{
    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client.start();

    EXPECT_TRUE(client.wait_connected(kTimeout));
}

TEST_F(LgsomeipApiRuntimeTest, offered_service_becomes_available_and_unavailable)
{
    Inbox requests;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);

    ASSERT_TRUE(availability.wait(true, kTimeout));
    EXPECT_TRUE(client->is_service_available(kService, kInstance));

    server->stop_offer_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(false, kTimeout));
    EXPECT_FALSE(client->is_service_available(kService, kInstance));

    server->offer_service(kService, kInstance, kMajor, kMinor);
    EXPECT_TRUE(availability.wait(true, kTimeout));
}

TEST_F(LgsomeipApiRuntimeTest, released_service_reports_no_further_availability_updates)
{
    Inbox requests;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    server->stop_offer_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(false, kTimeout));

    client->release_service(kService, kInstance);
    const auto updates = availability.updates();
    server->offer_service(kService, kInstance, kMajor, kMinor);

    EXPECT_FALSE(availability.wait_for_update(updates, kQuietPeriod));
    EXPECT_FALSE(availability.available());
}

TEST_F(LgsomeipApiRuntimeTest, request_reaches_the_provider_and_the_response_returns_with_all_fields)
{
    Inbox requests;
    Inbox responses;
    Availability availability;
    const api::Payload payload{0x01, 0x02, 0x03, 0xFF};

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kMethod, responses.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    client->send(make_request(payload));

    ASSERT_TRUE(responses.wait_for_count(1, kTimeout));
    const auto received_requests = requests.messages();
    const auto received_responses = responses.messages();
    ASSERT_EQ(received_requests.size(), 1U);
    ASSERT_EQ(received_responses.size(), 1U);

    const auto& request = *received_requests.front();
    EXPECT_EQ(request.service, kService);
    EXPECT_EQ(request.instance, kInstance);
    EXPECT_EQ(request.method, kMethod);
    EXPECT_EQ(request.client, kClientClientId);
    EXPECT_NE(request.session, 0U);
    EXPECT_EQ(request.interface_version, kMajor);
    EXPECT_EQ(request.type, api::MessageType::Request);
    EXPECT_EQ(request.payload, payload);

    const auto& response = *received_responses.front();
    EXPECT_EQ(response.service, kService);
    EXPECT_EQ(response.instance, kInstance);
    EXPECT_EQ(response.method, kMethod);
    EXPECT_EQ(response.client, kClientClientId);
    EXPECT_EQ(response.session, request.session);
    EXPECT_EQ(response.interface_version, kMajor);
    EXPECT_EQ(response.type, api::MessageType::Response);
    EXPECT_EQ(response.return_code, api::ReturnCode::Ok);
    EXPECT_EQ(response.payload, payload);
}

TEST_F(LgsomeipApiRuntimeTest, empty_payload_is_preserved)
{
    Inbox requests;
    Inbox responses;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kMethod, responses.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    client->send(make_request({}));

    ASSERT_TRUE(responses.wait_for_count(1, kTimeout));
    EXPECT_TRUE(requests.messages().front()->payload.empty());
    EXPECT_TRUE(responses.messages().front()->payload.empty());
}

TEST_F(LgsomeipApiRuntimeTest, sessions_are_generated_for_requests_with_a_zero_session)
{
    Inbox requests;
    Inbox responses;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kMethod, responses.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    client->send(make_request({0x01}));
    client->send(make_request({0x02}));

    ASSERT_TRUE(responses.wait_for_count(2, kTimeout));
    const auto received = responses.messages();
    EXPECT_NE(received[0]->session, 0U);
    EXPECT_NE(received[1]->session, 0U);
    EXPECT_NE(received[0]->session, received[1]->session);
}

TEST_F(LgsomeipApiRuntimeTest, unsupported_interface_version_is_answered_with_an_error)
{
    Inbox requests;
    Inbox responses;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kMethod, responses.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    client->send(make_request({0x01}, kMajor + 1));

    ASSERT_TRUE(responses.wait_for_count(1, kTimeout));
    const auto& error = *responses.messages().front();
    EXPECT_EQ(error.service, kService);
    EXPECT_EQ(error.method, kMethod);
    EXPECT_EQ(error.client, kClientClientId);
    EXPECT_EQ(error.type, api::MessageType::Error);
    EXPECT_EQ(error.return_code, api::ReturnCode::WrongInterfaceVersion);
    EXPECT_TRUE(error.payload.empty());
}

TEST_F(LgsomeipApiRuntimeTest, unregistered_message_handler_receives_no_further_messages)
{
    Inbox requests;
    Inbox responses;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kMethod, responses.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    client->send(make_request({0x01}));
    ASSERT_TRUE(responses.wait_for_count(1, kTimeout));

    client->unregister_message_handler(kService, kInstance, kMethod);
    client->send(make_request({0x02}));

    ASSERT_TRUE(requests.wait_for_count(2, kTimeout));
    EXPECT_FALSE(responses.wait_for_count(2, kQuietPeriod));
}

TEST_F(LgsomeipApiRuntimeTest, event_is_delivered_while_subscribed_only)
{
    Inbox events;
    Availability availability;
    SubscriptionLog subscriptions;

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kEvent, events.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_event(kService, kInstance, kEvent, {kEventGroup});
    client->request_service(kService, kInstance, kMajor, kMinor);

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_subscription_handler(kService, kInstance, kEventGroup, subscriptions.handler(true));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_event(kService, kInstance, kEvent, {kEventGroup});
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ASSERT_TRUE(availability.wait(true, kTimeout));
    client->subscribe(kService, kInstance, kEventGroup, kMajor);
    ASSERT_TRUE(subscriptions.wait_for(kClientClientId, true, kTimeout));

    api::Payload payload;
    for (std::uint8_t value = 0; value < 5; ++value)
    {
        payload.push_back(value);
        server->notify(kService, kInstance, kEvent, payload);
    }

    ASSERT_TRUE(events.wait_for_count(5, kTimeout));
    // Handlers run on a thread pool and may observe a newer payload of the event than the
    // one that triggered them. Therefore, the test checks every message for being one of the
    // published payloads and the last published payload for being delivered.
    const auto received = events.messages();
    ASSERT_EQ(received.size(), 5U);
    bool last_payload_received = false;
    for (const auto& message : received)
    {
        EXPECT_EQ(message->service, kService);
        EXPECT_EQ(message->instance, kInstance);
        EXPECT_EQ(message->method, kEvent);
        EXPECT_EQ(message->type, api::MessageType::Notification);
        ASSERT_GE(message->payload.size(), 1U);
        ASSERT_LE(message->payload.size(), 5U);
        EXPECT_EQ(message->payload.back(), static_cast<std::uint8_t>(message->payload.size() - 1));
        last_payload_received = last_payload_received || message->payload == payload;
    }
    EXPECT_TRUE(last_payload_received);

    // The provider is not informed about an unsubscription of a local application,
    // so the test waits for the daemon to process it before it publishes again.
    client->unsubscribe(kService, kInstance, kEventGroup);
    std::this_thread::sleep_for(kUnsubscribeSettleTime);
    events.clear();

    server->notify(kService, kInstance, kEvent, {0xEE});

    EXPECT_FALSE(events.wait_for_count(1, kQuietPeriod));
}

TEST_F(LgsomeipApiRuntimeTest, provider_subscription_handler_can_reject_a_subscription)
{
    Inbox events;
    Availability availability;
    SubscriptionLog subscriptions;

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kEvent, events.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_event(kService, kInstance, kEvent, {kEventGroup});
    client->request_service(kService, kInstance, kMajor, kMinor);

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_subscription_handler(kService, kInstance, kEventGroup, subscriptions.handler(false));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_event(kService, kInstance, kEvent, {kEventGroup});
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ASSERT_TRUE(availability.wait(true, kTimeout));
    client->subscribe(kService, kInstance, kEventGroup, kMajor);
    ASSERT_TRUE(subscriptions.wait_for(kClientClientId, true, kTimeout));

    api::Payload payload;
    for (std::uint8_t value = 0; value < 3; ++value)
    {
        payload.push_back(value);
        server->notify(kService, kInstance, kEvent, payload);
    }

    EXPECT_FALSE(events.wait_for_count(1, kQuietPeriod));
}

TEST_F(LgsomeipApiRuntimeTest, request_without_return_gets_no_response)
{
    Inbox requests;
    Inbox responses;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kMethod, responses.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_service(kService, kInstance, kMajor, kMinor);
    ASSERT_TRUE(availability.wait(true, kTimeout));

    auto request = make_request({0x0A, 0x0B});
    request.type = api::MessageType::RequestNoReturn;
    client->send(request);

    ASSERT_TRUE(requests.wait_for_count(1, kTimeout));
    const auto& received = *requests.messages().front();
    EXPECT_EQ(received.type, api::MessageType::RequestNoReturn);
    EXPECT_EQ(received.payload, request.payload);
    EXPECT_FALSE(responses.wait_for_count(1, kQuietPeriod));
}

TEST_F(LgsomeipApiRuntimeTest, find_service_reports_an_offered_service)
{
    Inbox requests;
    Availability availability;

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_message_handler(kService, kInstance, kMethod, echo_handler(server.get(), requests));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->find_service(kService, kInstance);

    EXPECT_TRUE(availability.wait(true, kTimeout));
}

TEST_F(LgsomeipApiRuntimeTest, event_is_not_delivered_after_the_provider_stops_offering_it)
{
    Inbox events;
    Availability availability;
    SubscriptionLog subscriptions;

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kEvent, events.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_event(kService, kInstance, kEvent, {kEventGroup});
    client->request_service(kService, kInstance, kMajor, kMinor);

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_subscription_handler(kService, kInstance, kEventGroup, subscriptions.handler(true));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_event(kService, kInstance, kEvent, {kEventGroup});
    server->offer_service(kService, kInstance, kMajor, kMinor);

    ASSERT_TRUE(availability.wait(true, kTimeout));
    client->subscribe(kService, kInstance, kEventGroup, kMajor);
    ASSERT_TRUE(subscriptions.wait_for(kClientClientId, true, kTimeout));

    server->notify(kService, kInstance, kEvent, {0x01});
    ASSERT_TRUE(events.wait_for_count(1, kTimeout));

    server->stop_offer_event(kService, kInstance, kEvent);
    events.clear();
    EXPECT_NO_THROW(server->notify(kService, kInstance, kEvent, {0x02}));

    EXPECT_FALSE(events.wait_for_count(1, kQuietPeriod));
}

TEST_F(LgsomeipApiRuntimeTest, unchanged_field_value_is_published_again_only_when_forced)
{
    Inbox events;
    Availability availability;
    SubscriptionLog subscriptions;

    ApiApp client("request-1", config_path_);
    ASSERT_TRUE(client.init());
    client->register_availability_handler(kService, kInstance, availability.handler());
    client->register_message_handler(kService, kInstance, kField, events.handler());
    client.start();
    ASSERT_TRUE(client.wait_connected(kTimeout));
    client->request_event(kService, kInstance, kField, {kEventGroup});
    client->request_service(kService, kInstance, kMajor, kMinor);

    ApiApp server("response", config_path_);
    ASSERT_TRUE(server.init());
    server->register_subscription_handler(kService, kInstance, kEventGroup, subscriptions.handler(true));
    server.start();
    ASSERT_TRUE(server.wait_connected(kTimeout));
    server->offer_event(kService, kInstance, kField, {kEventGroup}, true);
    server->offer_service(kService, kInstance, kMajor, kMinor);
    // The field has a value before the consumer subscribes, so that its initial value is sent.
    server->notify(kService, kInstance, kField, {0x01});

    ASSERT_TRUE(availability.wait(true, kTimeout));
    client->subscribe(kService, kInstance, kEventGroup, kMajor);
    ASSERT_TRUE(subscriptions.wait_for(kClientClientId, true, kTimeout));
    ASSERT_TRUE(events.wait_for_count(1, kTimeout));
    EXPECT_EQ(events.messages().front()->payload, api::Payload{0x01});

    server->notify(kService, kInstance, kField, {0x01});
    EXPECT_FALSE(events.wait_for_count(2, kQuietPeriod));

    server->notify(kService, kInstance, kField, {0x01}, true);
    EXPECT_TRUE(events.wait_for_count(2, kTimeout));
}

}  // namespace
