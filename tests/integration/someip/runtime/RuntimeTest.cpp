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

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <condition_variable>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <chrono>
#include <thread>
#include <exception/Exception.h>

#include <runtime/ApplicationManager.h>
#include <runtime/ServiceManager.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;
using namespace std::placeholders;

#define SOMEIP_SERVICE_ID 0x1001
#define SOMEIP_INSTANCE_ID 0x0001
#define SOMEIP_METHOD_ID 0x0001
#define SOMEIP_MAJOR_VERSION 0x01
#define SOMEIP_MINOR_VERSION 0x000000

#define SOMEIP_EVENT_GROUP 0x4455
#define SOMEIP_EVENT_ID_1 0x8777
#define SOMEIP_EVENT_ID_2 0x8778

class RuntimeTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        path_ = LGSOMEIP_TEST_CONFIG_DIR "/";
        missing_config_path_ = path_ + "config.json_no_file";
        config_path_ = path_ + "someip_config.json";
        invalid_config_path_ = path_ + "someip_config.json_error";
    }

    std::string daemon_name_ = "daemon";

    std::string path_;
    std::string missing_config_path_;
    std::string config_path_;
    std::string invalid_config_path_;

    std::string server_app_ = "response";
    std::string client_app_ = "request-1";
};

class DaemonSample {
public:
    DaemonSample(std::string name, std::string path) : service_manager_(std::make_shared<ServiceManager>(name, path)) {
        service_manager_->init();
        service_manager_->start();
    }

    ~DaemonSample() {
        deinit();
    }

    void deinit() {
        if (service_manager_ != nullptr) {
            service_manager_->stop();
            service_manager_.reset();
        }
    }

private:
    std::shared_ptr<ServiceManager> service_manager_{nullptr};
};

// Server application test helper.
class ServerAppSample {
public:
    ServerAppSample(std::string name, std::string path)
        : application_manager_(std::make_shared<ApplicationManager>(name, path)) {
        application_manager_->init();
        application_manager_->register_application_state_handler(std::bind(&ServerAppSample::on_state, this, _1));
        application_manager_->start();
    }

    ~ServerAppSample() {
        deinit();
    }

    void deinit() {
        if (application_manager_ != nullptr) {
            application_manager_->stop();
            application_manager_->join();
            application_manager_.reset();
        }
    }

    void offer_service(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID,
                       std::uint8_t major = SOMEIP_MAJOR_VERSION, std::uint32_t minor = SOMEIP_MINOR_VERSION) {
        application_manager_->offer_service(sid, iid, major, minor);
    }

    void stop_offer_service(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID,
                            std::uint8_t major = SOMEIP_MAJOR_VERSION, std::uint32_t minor = SOMEIP_MINOR_VERSION) {
        application_manager_->stop_offer_service(sid, iid, major, minor);
    }

    void register_message_handler(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID,
                                  std::uint16_t mid = SOMEIP_METHOD_ID) {
        application_manager_->register_message_handler(sid, iid, mid, std::bind(&ServerAppSample::on_message, this, _1),
                                                       true);
    }

    void offer_event(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID) {
        std::set<std::uint16_t> eventgroups{SOMEIP_EVENT_GROUP};
        application_manager_->offer_event(sid, iid, SOMEIP_EVENT_ID_1, eventgroups);
        application_manager_->offer_event(sid, iid, SOMEIP_EVENT_ID_2, eventgroups);
    }

    void on_state(std::uint16_t state) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            connected_ = state == SOMEIP_APPLICATION_REGISTERED;
        }
        state_condition_.notify_all();
    }

    void notify(std::shared_ptr<Payload> msg, std::uint16_t sid = SOMEIP_SERVICE_ID,
                std::uint16_t iid = SOMEIP_INSTANCE_ID, std::uint16_t eid = SOMEIP_EVENT_ID_1) {
        if (eid == SOMEIP_EVENT_ID_1 || eid == SOMEIP_EVENT_ID_2) {
            application_manager_->notify(sid, iid, eid, msg);
        }
    }

    void on_message(std::shared_ptr<Message> msg) {
        std::cout << "ServerAppSample::on_message Called!!" << std::endl;
        if (msg->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
            auto message = MessageBuilder::create_response_message(*msg);
            std::cout << "ServerAppSample::on_message / payload len = " << msg->get_payload_type()->get_length()
                      << std::endl;

            if (msg->get_payload_type()->get_length() == 0) {
                std::uint8_t payload[] = "OK";
                message->set_payload(payload, 2);
            } else {
                message->set_payload(msg->get_payload_type());
            }

            application_manager_->send(message);
        }
    }

    bool get_connected() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return connected_;
    }

    bool wait_connected(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        return state_condition_.wait_for(lock, timeout, [this] { return connected_; });
    }

private:
    std::shared_ptr<ApplicationManager> application_manager_{nullptr};

    mutable std::mutex state_mutex_;
    std::condition_variable state_condition_;
    bool connected_ = false;
};

// Client application test helper.
class ClientAppSample {
public:
    ClientAppSample(std::string name, std::string path)
        : application_manager_(std::make_shared<ApplicationManager>(name, path)) {
        application_manager_->init();
        application_manager_->register_application_state_handler(std::bind(&ClientAppSample::on_state, this, _1));
        application_manager_->start();
    }

    ~ClientAppSample() {
        deinit();
    }

    void deinit() {
        if (application_manager_ != nullptr) {
            application_manager_->stop();
            application_manager_->join();
            application_manager_.reset();
        }

        std::cout << "ClientAppSample::deinit() / end" << std::endl;
    }

    void on_state(std::uint16_t state) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            connected_ = state == SOMEIP_APPLICATION_REGISTERED;
        }
        state_condition_.notify_all();
    }

    void register_message_handler(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID,
                                  std::uint16_t mid = SOMEIP_METHOD_ID) {
        application_manager_->register_message_handler(sid, iid, mid,
                                                       std::bind(&ClientAppSample::on_message, this, _1));
    }

    void register_available_handler(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID) {
        application_manager_->register_availability_handler(
            sid, iid, std::bind(&ClientAppSample::on_availability, this, _1, _2, _3));
    }

    void request_service(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID,
                         std::uint8_t major = SOMEIP_MAJOR_VERSION, std::uint32_t minor = SOMEIP_MINOR_VERSION) {
        application_manager_->request_service(sid, iid, major, minor);
    }

    void release_service(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID) {
        application_manager_->release_service(sid, iid);
    }

    void request_event() {
        application_manager_->request_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1,
                                            {SOMEIP_EVENT_GROUP});
        application_manager_->request_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2,
                                            {SOMEIP_EVENT_GROUP});
    }

    void request_event_wrong_1() {
        application_manager_->request_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1 + 0x1000,
                                            {SOMEIP_EVENT_GROUP});
    }

    void request_event_wrong_2() {
        application_manager_->request_event(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_1,
                                            {SOMEIP_EVENT_GROUP + 0x1000});
    }

    void subscribe(bool enable = true) {
        if (enable) {
            application_manager_->subscribe(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_GROUP, 0x01);
        } else {
            application_manager_->unsubscribe(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_GROUP);
        }
    }

    void register_subscription_status_handler(std::uint16_t sid = SOMEIP_SERVICE_ID,
                                              std::uint16_t iid = SOMEIP_INSTANCE_ID) {
        application_manager_->register_subscription_status_handler(
            sid, iid, SOMEIP_EVENT_GROUP, SOMEIP_DEFAULT_ANY_EVENT,
            std::bind(&ClientAppSample::on_subscription_status, this, _1, _2, _3, _4, _5), true);
    }

    void subscribe_wrong() {
        application_manager_->subscribe(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_GROUP + 0x1000, 0x01);
    }

    std::shared_ptr<ApplicationManager> get_application() {
        return application_manager_;
    }

    bool get_available(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID) const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return get_available_locked(sid, iid);
    }

    bool wait_connected(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        return state_condition_.wait_for(lock, timeout, [this] { return connected_; });
    }

    bool wait_available(bool available, std::chrono::milliseconds timeout, std::uint16_t sid = SOMEIP_SERVICE_ID,
                        std::uint16_t iid = SOMEIP_INSTANCE_ID) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        return state_condition_.wait_for(
            lock, timeout, [this, available, sid, iid] { return get_available_locked(sid, iid) == available; });
    }

    std::size_t get_availability_update_count() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return availability_update_count_;
    }

    bool wait_availability_update(std::size_t previous_count, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        return state_condition_.wait_for(
            lock, timeout, [this, previous_count] { return availability_update_count_ > previous_count; });
    }

    bool wait_received_messages(std::size_t count, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        return state_condition_.wait_for(lock, timeout, [this, count] { return received_messages_.size() >= count; });
    }

    bool wait_subscription_status(std::uint16_t status, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        return state_condition_.wait_for(
            lock, timeout, [this, status] { return subscription_status_received_ && subscription_status_ == status; });
    }

    void clear_received_messages() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        received_messages_.clear();
    }

    std::vector<std::shared_ptr<Message>> get_all_received_messages() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return received_messages_;
    }

    bool get_connected() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return connected_;
    }

    void on_availability(std::uint16_t sid, std::uint16_t iid, bool available) {
        std::cout << "ClientAppSample::on_availability Called : " << available << std::endl;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            available_services_[sid][iid] = available;
            ++availability_update_count_;
        }
        state_condition_.notify_all();
    }

    void on_message(std::shared_ptr<Message> msg) {
        std::cout << "ClientAppSample::on_message Called" << std::endl;
        std::cout << "ClientAppSample::on_message / payload len = " << msg->get_payload_type()->get_length()
                  << std::endl;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            received_messages_.push_back(msg);
        }
        state_condition_.notify_all();
    }

    void on_subscription_status(std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t status) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            subscription_status_ = status;
            subscription_status_received_ = true;
        }
        state_condition_.notify_all();
    }

    void send_request(std::uint16_t sid = SOMEIP_SERVICE_ID, std::uint16_t iid = SOMEIP_INSTANCE_ID,
                      std::uint8_t major = SOMEIP_MAJOR_VERSION, std::shared_ptr<Payload> payload = nullptr) {
        std::cout << "ClientAppSample::send_message Called" << std::endl;
        sent_message_ = MessageBuilder::create_request_message(sid, iid, major);
        if (payload != nullptr) {
            sent_message_->set_payload(payload);
        }
        application_manager_->send(sent_message_);
    }

    std::shared_ptr<Message> get_send_message() {
        return sent_message_;
    }
    std::shared_ptr<Message> get_received_message() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (received_messages_.empty()) {
            return nullptr;
        }
        return received_messages_.back();
    }

private:
    bool get_available_locked(std::uint16_t sid, std::uint16_t iid) const {
        auto service_it = available_services_.find(sid);
        if (service_it == available_services_.end()) {
            return false;
        }

        auto instance_it = service_it->second.find(iid);
        if (instance_it == service_it->second.end()) {
            return false;
        }

        return instance_it->second;
    }

    std::shared_ptr<ApplicationManager> application_manager_{nullptr};
    std::vector<std::shared_ptr<Message>> received_messages_;
    std::shared_ptr<Message> sent_message_{nullptr};

    mutable std::mutex state_mutex_;
    std::condition_variable state_condition_;
    bool connected_ = false;
    std::size_t availability_update_count_ = 0;
    bool subscription_status_received_ = false;
    std::uint16_t subscription_status_ = 0;
    std::map<std::uint16_t, std::map<std::uint16_t, bool>> available_services_;
};

TEST_F(RuntimeTest, ServiceConfiguration1) {
    bool ret = false;

    try {
        ServiceManager service_manager(daemon_name_, missing_config_path_);
    } catch (const ConfigurationErrorException& e) {
        ret = true;
    } catch (const std::exception& e) {
        ret = true;
    }

    EXPECT_EQ(ret, false);
}

TEST_F(RuntimeTest, ServiceConfiguration2) {
    bool ret = false;
    try {
        ServiceManager service_manager(daemon_name_, invalid_config_path_);
        service_manager.init();
    } catch (const ConfigurationErrorException& e) {
        ret = true;
    } catch (const std::exception& e) {
    }

    EXPECT_EQ(ret, true);
}
TEST_F(RuntimeTest, ServiceConfiguration3) {
    bool ret = false;
    try {
        ServiceManager service_manager(daemon_name_, config_path_);
        service_manager.init();
        service_manager.start();
        service_manager.stop();

        EXPECT_EQ(service_manager.get_application_id(), 0x0000);
        EXPECT_EQ(service_manager.get_application_name(), "daemon");

        ret = true;
    } catch (const std::exception& e) {
    }

    EXPECT_EQ(ret, true);
}

TEST_F(RuntimeTest, ServiceConfiguration4_Multicast) {
    std::string multicast;
    try {
        ServiceManager service_manager(daemon_name_, config_path_);
        service_manager.init();

        auto service_info = service_manager.get_configuration()->get_service_info(0x1002, 0x0001);
        auto addr = service_info->get_multicast_address();
        if (addr != nullptr) {
            multicast = ((*addr)[0])->to_string();
        }
    } catch (const std::exception& e) {
    }

    EXPECT_EQ(multicast, "224.225.226.234:32344 (multicast)");
}
TEST_F(RuntimeTest, ApplicationConfiguration1) {
    bool ret = false;
    try {
        ApplicationManager application_manager("test1", config_path_);
    } catch (const ConfigurationErrorException& e) {
        ret = true;
    } catch (const std::exception& e) {
    }

    EXPECT_EQ(ret, false);
}
TEST_F(RuntimeTest, EventConfiguration) {
    int count = 0;

    DaemonSample daemon(daemon_name_, config_path_);

    // Start the client application and wait for daemon registration.
    ClientAppSample client1(client_app_, config_path_);
    ASSERT_TRUE(client1.wait_connected(std::chrono::seconds(10)));

    try {
        client1.request_event_wrong_1();
    } catch (const ConfigurationErrorException& e) {
        count++;
    } catch (const std::exception& e) {
    }

    try {
        client1.request_event_wrong_2();
    } catch (const ConfigurationErrorException& e) {
        count++;
    } catch (const std::exception& e) {
    }

    try {
        client1.subscribe_wrong();
    } catch (const ConfigurationErrorException& e) {
        count++;
    } catch (const std::exception& e) {
    }

    client1.deinit();
    daemon.deinit();
    EXPECT_EQ(count, 0);
}

TEST_F(RuntimeTest, OfferServiceTest) {
    try {
        DaemonSample daemon(daemon_name_, config_path_);

        // Start the server application and wait for daemon registration.
        ServerAppSample server1(server_app_, config_path_);
        ASSERT_TRUE(server1.wait_connected(std::chrono::seconds(10)));

        server1.register_message_handler();
        server1.offer_service();

        // Start the client application and wait for daemon registration.
        ClientAppSample client1(client_app_, config_path_);
        ASSERT_TRUE(client1.wait_connected(std::chrono::seconds(10)));

        client1.register_available_handler();
        client1.register_message_handler();
        client1.request_service();

        ASSERT_TRUE(client1.wait_available(true, std::chrono::seconds(10)));

        // Verify that withdrawing the offer reports the service unavailable.
        server1.stop_offer_service();
        ASSERT_TRUE(client1.wait_available(false, std::chrono::seconds(10)));

        server1.offer_service();
        ASSERT_TRUE(client1.wait_available(true, std::chrono::seconds(10)));

        client1.deinit();
        server1.deinit();
        daemon.deinit();
    } catch (const std::exception& e) {
        // Report unexpected exceptions as test failures.
        FAIL() << "Unexpected exception: " << e.what();
    }
}

TEST_F(RuntimeTest, RequestAndReleaseServiceTest) {
    const auto timeout = std::chrono::seconds(10);

    try {
        DaemonSample daemon(daemon_name_, config_path_);

        // Start the server application and wait for daemon registration.
        ServerAppSample server1(server_app_, config_path_);
        ASSERT_TRUE(server1.wait_connected(timeout));

        server1.register_message_handler();
        server1.offer_service();

        // Start the client application and wait for daemon registration.
        ClientAppSample client1(client_app_, config_path_);
        ASSERT_TRUE(client1.wait_connected(timeout));

        client1.register_available_handler();
        client1.register_message_handler();
        client1.request_service();

        ASSERT_TRUE(client1.wait_available(true, timeout));

        // Verify that withdrawing the offer reports the service unavailable.
        server1.stop_offer_service();
        ASSERT_TRUE(client1.wait_available(false, timeout));

        // Verify that releasing interest prevents future availability updates.
        client1.release_service();

        auto availability_updates = client1.get_availability_update_count();
        server1.offer_service();
        EXPECT_FALSE(client1.wait_availability_update(availability_updates, std::chrono::seconds(1)));
        EXPECT_FALSE(client1.get_available());

        client1.deinit();
        daemon.deinit();
    } catch (const std::exception& e) {
        // Report unexpected exceptions as test failures.
        FAIL() << "Unexpected exception: " << e.what();
    }
}

TEST_F(RuntimeTest, MethodTest1) {
    try {
        DaemonSample daemon(daemon_name_, config_path_);

        // Start the server application and wait for daemon registration.
        ServerAppSample server1(server_app_, config_path_);
        ASSERT_TRUE(server1.wait_connected(std::chrono::seconds(10)));

        server1.register_message_handler();
        server1.offer_service();

        // Start the client application and wait for daemon registration.
        ClientAppSample client1(client_app_, config_path_);
        ASSERT_TRUE(client1.wait_connected(std::chrono::seconds(10)));

        client1.register_available_handler();
        client1.register_message_handler();
        client1.request_service();

        ASSERT_TRUE(client1.wait_available(true, std::chrono::seconds(10)));

        // Send a method request and verify its response.
        client1.send_request();
        ASSERT_TRUE(client1.wait_received_messages(1, std::chrono::seconds(10)));

        auto send_message = client1.get_send_message();
        auto received_message = client1.get_received_message();
        ASSERT_NE(send_message, nullptr);
        ASSERT_NE(received_message, nullptr);
        EXPECT_EQ(send_message->get_request_id(), received_message->get_request_id());
        EXPECT_EQ(received_message->get_payload_type()->get_length(), 2);

        client1.deinit();
        server1.deinit();
        daemon.deinit();
    } catch (const std::exception& e) {
        // Report unexpected exceptions as test failures.
        FAIL() << "Unexpected exception: " << e.what();
    }
}

TEST_F(RuntimeTest, MethodTest2) {
    try {
        DaemonSample daemon(daemon_name_, config_path_);

        // Start the server application and wait for daemon registration.
        ServerAppSample server1(server_app_, config_path_);
        ASSERT_TRUE(server1.wait_connected(std::chrono::seconds(10)));

        server1.register_message_handler();
        server1.offer_service();

        // Start the client application and wait for daemon registration.
        ClientAppSample client1(client_app_, config_path_);
        ASSERT_TRUE(client1.wait_connected(std::chrono::seconds(10)));

        client1.register_available_handler(SOMEIP_SERVICE_ID, SOMEIP_DEFAULT_ANY_INSTANCE);
        client1.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_DEFAULT_ANY_INSTANCE, SOMEIP_DEFAULT_ANY_METHOD);
        client1.request_service(SOMEIP_SERVICE_ID, SOMEIP_DEFAULT_ANY_INSTANCE);

        // Verify that the availability callback reports the offer.
        ASSERT_TRUE(client1.wait_available(true, std::chrono::seconds(10)));

        // Send a method request and verify its response.
        client1.send_request();
        ASSERT_TRUE(client1.wait_received_messages(1, std::chrono::seconds(10)));
        auto send_message = client1.get_send_message();
        auto received_message = client1.get_received_message();
        ASSERT_NE(send_message, nullptr);
        ASSERT_NE(received_message, nullptr);
        EXPECT_EQ(send_message->get_request_id(), received_message->get_request_id());
        EXPECT_EQ(received_message->get_payload_type()->get_length(), 2);

        // Send a request with an unsupported interface version and verify its error.
        client1.send_request(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION + 1);
        ASSERT_TRUE(client1.wait_received_messages(2, std::chrono::seconds(10)));
        send_message = client1.get_send_message();
        received_message = client1.get_received_message();
        ASSERT_NE(send_message, nullptr);
        ASSERT_NE(received_message, nullptr);
        EXPECT_EQ(send_message->get_request_id(), received_message->get_request_id());
        EXPECT_EQ(received_message->get_message_type(), SOMEIP_MESSAGE_TYPE::ERROR);
        EXPECT_EQ(received_message->get_return_code(), SOMEIP_RETURN_CODE::E_WRONG_INTERFACE_VERSION);
        EXPECT_EQ(received_message->get_payload_type()->get_length(), 0);

        client1.deinit();
        server1.deinit();
        daemon.deinit();
    } catch (const std::exception& e) {
        // Report unexpected exceptions as test failures.
        FAIL() << "Unexpected exception: " << e.what();
    }
}

TEST_F(RuntimeTest, EventTest1) {
    const auto timeout = std::chrono::seconds(10);

    try {
        DaemonSample daemon(daemon_name_, config_path_);

        // Start the client application and wait for daemon registration.
        ClientAppSample client1(client_app_, config_path_);
        ASSERT_TRUE(client1.wait_connected(timeout));

        client1.register_available_handler(SOMEIP_SERVICE_ID, SOMEIP_DEFAULT_ANY_INSTANCE);
        client1.register_message_handler(SOMEIP_SERVICE_ID, SOMEIP_DEFAULT_ANY_INSTANCE, SOMEIP_DEFAULT_ANY_METHOD);
        client1.register_subscription_status_handler();
        client1.request_event();
        client1.request_service(SOMEIP_SERVICE_ID, SOMEIP_DEFAULT_ANY_INSTANCE);

        // Start the server application and wait for daemon registration.
        ServerAppSample server1(server_app_, config_path_);
        ASSERT_TRUE(server1.wait_connected(timeout));

        server1.register_message_handler();
        server1.offer_event();
        server1.offer_service();

        // Verify that the availability callback reports the offer.
        ASSERT_TRUE(client1.wait_available(true, timeout));
        client1.subscribe();
        ASSERT_TRUE(client1.wait_subscription_status(0, timeout));

        std::shared_ptr<Payload> payload = std::make_shared<Payload>();
        for (char ch = '0'; ch < '5'; ch++) {
            payload->append(ch);
            server1.notify(payload, SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2);
        }

        ASSERT_TRUE(client1.wait_received_messages(5, timeout));
        EXPECT_EQ(client1.get_all_received_messages().size(), 5U);

        client1.clear_received_messages();
        client1.subscribe(false);
        ASSERT_TRUE(client1.wait_subscription_status(0xff, timeout));

        payload = std::make_shared<Payload>();
        for (char ch = '5'; ch < '9'; ch++) {
            payload->append(ch);
            server1.notify(payload, SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_EVENT_ID_2);
        }

        EXPECT_FALSE(client1.wait_received_messages(1, std::chrono::seconds(1)));
        EXPECT_EQ(client1.get_all_received_messages().size(), 0U);

        client1.deinit();
        server1.deinit();
        daemon.deinit();
    } catch (const std::exception& e) {
        // Report unexpected exceptions as test failures.
        FAIL() << "Unexpected exception: " << e.what();
    }
}

#if defined(ENABLE_SOMEIP_TP)
TEST_F(RuntimeTest, SOMEIPTPTest1) {
    const auto timeout = std::chrono::seconds(10);

    try {
        DaemonSample daemon(daemon_name_, config_path_);

        // Start the server application and wait for daemon registration.
        ServerAppSample server1(server_app_, config_path_);
        ASSERT_TRUE(server1.wait_connected(timeout));

        server1.register_message_handler();
        server1.offer_service();

        // Start the client application and wait for daemon registration.
        ClientAppSample client1(client_app_, config_path_);
        ASSERT_TRUE(client1.wait_connected(timeout));

        client1.register_available_handler();
        client1.register_message_handler();
        client1.request_service();

        ASSERT_TRUE(client1.wait_available(true, timeout));

        // Send a method request and verify its response.
        std::shared_ptr<Payload> payload = std::make_shared<Payload>();

        std::vector<std::uint8_t> data;
        for (int i = 0; i < 5000; i++) {
            data.push_back(0x11);
        }
        payload->append(data);

        client1.send_request(SOMEIP_SERVICE_ID, SOMEIP_INSTANCE_ID, SOMEIP_MAJOR_VERSION, payload);
        ASSERT_TRUE(client1.wait_received_messages(1, timeout));

        auto send_message = client1.get_send_message();
        auto all_received_messages = client1.get_all_received_messages();

        ASSERT_NE(send_message, nullptr);
        ASSERT_FALSE(all_received_messages.empty());

        std::size_t total_payload_length = 0;

        for (const auto& received_message : all_received_messages) {
            total_payload_length += received_message->get_payload_type()->get_length();
        }

        EXPECT_EQ(send_message->get_request_id(), all_received_messages.back()->get_request_id());
        EXPECT_EQ(send_message->get_payload_type()->get_length(), total_payload_length);

        client1.deinit();
        server1.deinit();
        daemon.deinit();
    } catch (const std::exception& e) {
        // Report unexpected exceptions as test failures.
        FAIL() << "Unexpected exception: " << e.what();
    }
}
#endif // ENABLE_SOMEIP_TP
