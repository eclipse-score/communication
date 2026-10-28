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

#ifndef LG_SOMEIP_ENDPOINT_ENDPOINTMESSAGEPASSING_H
#define LG_SOMEIP_ENDPOINT_ENDPOINTMESSAGEPASSING_H

#include <cstdint>

#include <endpoint/EndpointBase.h>

#include <messagepassing/MessagePassingServer.h>
#include <messagepassing/MessagePassingSender.h>

namespace lgsomeip {

class EndpointMessagePassingServer;
class EndpointMessagePassingReceiver;
class EndpointMessagePassingSender;

class EndPointMessagePassingListener {
public:
#if !defined(ENABLE_SOMEIP_IPC)
    virtual void on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                            std::shared_ptr<Endpoint> client_endpoint) = 0;

    virtual void on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint) = 0;
#else
    virtual void on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                            std::shared_ptr<Endpoint> client_endpoint, int connection_id = 0xff) = 0;

    virtual void on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint, int connection_id = 0xff) = 0;
#endif // ENABLE_SOMEIP_IPC

    virtual void on_message_passing_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                            std::size_t message_length) = 0;

    virtual void on_message_passing_timer(const int32_t id) {}
};

class EndpointMessagePassingServer : public EndpointBase,
                                     lgsomeip::osabstraction::MessagePassingConnectionListener,
                                     lgsomeip::osabstraction::MessagePassingTimerListener {
public:
    EndpointMessagePassingServer(EndPointMessagePassingListener* listener, const std::string& name);
    virtual ~EndpointMessagePassingServer();

    virtual void start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer = nullptr) override;

    virtual void stop_listen() override;

    virtual void send_message(const std::uint8_t* message, std::size_t size,
                              std::shared_ptr<lgsomeip::osabstraction::Address> address = nullptr) override;

    virtual void on_connect(const lgsomeip::osabstraction::MessagePassingConnectionInformation& info) override;

    virtual void on_disconnect(const lgsomeip::osabstraction::MessagePassingConnectionInformation& info) override;

    virtual std::shared_ptr<lgsomeip::osabstraction::Address> get_sender_address() override;

    void set_timer(const std::int32_t id, const std::uint32_t interval_milliseconds, const bool periodic);

    void kill_timer(const std::int32_t id);

    virtual void on_timer(const std::int32_t id);

private:
    EndPointMessagePassingListener* listener_;

    lgsomeip::osabstraction::MessagePassingServer message_passing_server_;

    std::map<int, std::shared_ptr<EndpointMessagePassingReceiver>> receivers_;
};

class EndpointMessagePassingReceiver : public EndpointBase, lgsomeip::osabstraction::MessagePassingDataListener {
public:
    EndpointMessagePassingReceiver(EndPointMessagePassingListener* listener);
    virtual ~EndpointMessagePassingReceiver();

    virtual void start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer = nullptr) override;

    virtual void stop_listen() override;

    virtual void send_message(const std::uint8_t* message, std::size_t size,
                              std::shared_ptr<lgsomeip::osabstraction::Address> address = nullptr) override;

    virtual void on_message(std::uint8_t* data, std::size_t size) override;

    virtual void on_error() override;

    virtual std::shared_ptr<lgsomeip::osabstraction::Address> get_sender_address() override;

    void set_connection_information(lgsomeip::osabstraction::MessagePassingServer* message_passing_server,
                                    const lgsomeip::osabstraction::MessagePassingConnectionInformation& info);

    int get_connection_id();

private:
    EndPointMessagePassingListener* listener_;

    lgsomeip::osabstraction::MessagePassingServer* message_passing_server_;
    lgsomeip::osabstraction::MessagePassingConnectionInformation connection_information_;
};

class EndpointMessagePassingSender : public EndpointBase {
public:
    EndpointMessagePassingSender(const std::string& server_name);
    virtual ~EndpointMessagePassingSender();

    virtual bool is_connected();
    virtual void start_listen(std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer = nullptr) override;

    virtual void stop_listen() override;

    virtual void send_message(const std::uint8_t* message, std::size_t size,
                              std::shared_ptr<lgsomeip::osabstraction::Address> address = nullptr) override;

    virtual std::shared_ptr<lgsomeip::osabstraction::Address> get_sender_address() override;

private:
    lgsomeip::osabstraction::MessagePassingSender message_passing_sender_;
    bool is_connected_;
};

} // namespace lgsomeip

#endif // LG_SOMEIP_ENDPOINT_ENDPOINTMESSAGEPASSING_H
