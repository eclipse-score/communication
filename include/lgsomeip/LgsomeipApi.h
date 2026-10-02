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

/**
 * @file LgsomeipApi.h
 * @brief Public C++ API for LG SOME/IP applications.
 */

#ifndef LG_SOMEIP_LGSOMEIP_API_H
#define LG_SOMEIP_LGSOMEIP_API_H

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace lgsomeip {
namespace api {

/** @brief SOME/IP service identifier. */
using service_t = std::uint16_t;
/** @brief SOME/IP service-instance identifier. */
using instance_t = std::uint16_t;
/** @brief SOME/IP method identifier. */
using method_t = std::uint16_t;
/** @brief SOME/IP event identifier. */
using event_t = std::uint16_t;
/** @brief SOME/IP event-group identifier. */
using eventgroup_t = std::uint16_t;
/** @brief Client identifier in the SOME/IP request ID. */
using client_t = std::uint16_t;
/** @brief Major service-interface version. */
using major_version_t = std::uint8_t;
/** @brief Minor service-interface version. */
using minor_version_t = std::uint32_t;
/** @brief One byte of a serialized SOME/IP payload. */
using byte_t = std::uint8_t;
/** @brief Raw serialized payload bytes; application-specific encoding is caller-owned. */
using Payload = std::vector<byte_t>;

/** @brief Default major version for service offers and event subscriptions. */
constexpr major_version_t kDefaultMajorVersion = 0;
/** @brief Default minor version for service offers. */
constexpr minor_version_t kDefaultMinorVersion = 0;
/** @brief Wildcard major version used to match any service version. */
constexpr major_version_t kAnyMajorVersion = 0xFF;
/** @brief Wildcard minor version used to match any service version. */
constexpr minor_version_t kAnyMinorVersion = 0xFFFFFFFF;
/** @brief Wildcard event ID used to subscribe to every requested event in a group. */
constexpr event_t kAnyEvent = 0xFFFF;

/**
 * @brief SOME/IP message types encoded in the message header.
 *
 * The underlying values match the SOME/IP wire specification, including
 * Transport Protocol variants.
 */
enum class MessageType : std::uint8_t {
    Request = 0x00,
    RequestNoReturn = 0x01,
    Notification = 0x02,
    TpRequest = 0x20,
    TpRequestNoReturn = 0x21,
    TpNotification = 0x22,
    Response = 0x80,
    Error = 0x81,
    TpResponse = 0xA0,
    TpError = 0xA1
};

/**
 * @brief SOME/IP return codes encoded in responses and error messages.
 *
 * The underlying values match the SOME/IP wire specification.
 */
enum class ReturnCode : std::uint8_t {
    Ok = 0x00,
    NotOk = 0x01,
    UnknownService = 0x02,
    UnknownMethod = 0x03,
    NotReady = 0x04,
    NotReachable = 0x05,
    Timeout = 0x06,
    WrongProtocolVersion = 0x07,
    WrongInterfaceVersion = 0x08,
    MalformedMessage = 0x09,
    WrongMessageType = 0x0A,
    E2ERepeated = 0x0B,
    E2EWrongSequence = 0x0C,
    E2E = 0x0D,
    E2ENotAvailable = 0x0E,
    E2ENoNewData = 0x0F
};

/**
 * @brief Public SOME/IP message value.
 *
 * This type contains the application-visible SOME/IP header fields and the
 * serialized payload. The API does not serialize application-specific data;
 * callers must encode data into payload before sending and decode it after
 * receiving a message.
 *
 * For outgoing requests, the application manager supplies the local client ID
 * and generates a nonzero session when the supplied session is zero. Because
 * send() accepts a const Message, generated request IDs are not written back
 * to the caller's object. Use the session delivered in the response callback
 * when correlating a response with a request.
 *
 * @var Message::service Service identifier from the SOME/IP message ID.
 * @var Message::instance Service-instance identifier used by routing and Service Discovery.
 * @var Message::method Method identifier for requests and responses, or event identifier for notifications.
 * @var Message::client Client portion of the SOME/IP request ID.
 * @var Message::session Session portion of the request ID used for correlation.
 * @var Message::interface_version Major interface version carried in the SOME/IP header.
 * @var Message::type SOME/IP message type, such as request, response, error, or notification.
 * @var Message::return_code Return or error code carried by a response.
 * @var Message::payload Serialized application payload bytes.
 */
struct Message {
    service_t service{0};
    instance_t instance{0};
    method_t method{0};
    client_t client{0};
    std::uint16_t session{0};
    major_version_t interface_version{kDefaultMajorVersion};
    MessageType type{MessageType::Request};
    ReturnCode return_code{ReturnCode::Ok};
    Payload payload;
};

/** @brief Callback invoked for a received request, response, notification, or error. */
using MessageHandler = std::function<void(const std::shared_ptr<Message>&)>;
/** @brief Callback invoked when a matching service instance becomes available or unavailable. */
using AvailabilityHandler = std::function<void(service_t, instance_t, bool)>;
/** @brief Callback invoked when the application connects to or disconnects from the routing daemon. */
using ApplicationStateHandler = std::function<void(bool)>;
/** @brief Callback that accepts or rejects a client's event-group subscription. */
using SubscriptionHandler = std::function<bool(client_t, bool)>;

/**
 * @brief Lifecycle and communication interface for one SOME/IP application.
 *
 * Call init() before start(). Register callbacks before start() because message
 * and state callbacks may run asynchronously. start() is non-blocking; use
 * join() to wait until stop() ends message processing.
 */
class Application {
public:
    /** @brief Destroy the application and its owned communication resources. */
    virtual ~Application();

    /**
     * @brief Return the configured application name.
     * @return The application name used for configuration and routing.
     */
    virtual const std::string& name() const = 0;

    /**
     * @brief Return the local SOME/IP client ID.
     * @return The client identifier assigned to this application.
     */
    virtual client_t client_id() const = 0;

    /**
     * @brief Initialize configuration and local communication resources.
     * @return true when initialization succeeds; otherwise false.
     */
    virtual bool init() = 0;

    /**
     * @brief Start message processing.
     *
     * This method returns after processing has been started. It does not wait
     * for the application to stop.
     */
    virtual void start() = 0;

    /**
     * @brief Wait for message processing to finish.
     *
     * This method blocks until stop() is called.
     */
    virtual void join() = 0;

    /**
     * @brief Stop message processing.
     *
     * After this method returns, join() can complete.
     */
    virtual void stop() = 0;

    /**
     * @brief Offer a service instance to other applications.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param major Major service-interface version.
     * @param minor Minor service-interface version.
     * @note Call after the application-state handler reports daemon registration.
     */
    virtual void offer_service(service_t service, instance_t instance,
                               major_version_t major = kDefaultMajorVersion,
                               minor_version_t minor = kDefaultMinorVersion) = 0;

    /**
     * @brief Withdraw an offered service instance.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param major Major version, or kAnyMajorVersion for all major versions.
     * @param minor Minor version, or kAnyMinorVersion for all minor versions.
     */
    virtual void stop_offer_service(service_t service, instance_t instance,
                                    major_version_t major = kAnyMajorVersion,
                                    minor_version_t minor = kAnyMinorVersion) = 0;

    /**
     * @brief Send a SOME/IP Service Discovery query.
     * @param service Service identifier to find.
     * @param instance Instance identifier to find.
     * @param major Major version to find, or kAnyMajorVersion.
     * @param minor Minor version to find, or kAnyMinorVersion.
     */
    virtual void find_service(service_t service, instance_t instance,
                              major_version_t major = kAnyMajorVersion,
                              minor_version_t minor = kAnyMinorVersion) = 0;

    /**
     * @brief Register persistent interest in a service instance.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param major Required major version, or kAnyMajorVersion.
     * @param minor Required minor version, or kAnyMinorVersion.
     * @note The request is retried after a routing-daemon reconnect.
     */
    virtual void request_service(service_t service, instance_t instance,
                                 major_version_t major = kAnyMajorVersion,
                                 minor_version_t minor = kAnyMinorVersion) = 0;

    /**
     * @brief Release persistent interest in a service instance.
     * @param service Service identifier.
     * @param instance Instance identifier.
     */
    virtual void release_service(service_t service, instance_t instance) = 0;

    /**
     * @brief Check whether a service instance is currently available.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param major Major version to match, or kAnyMajorVersion.
     * @param minor Minor version to match, or kAnyMinorVersion.
     * @return true if a matching service instance is available.
     */
    virtual bool is_service_available(service_t service, instance_t instance,
                                      major_version_t major = kAnyMajorVersion,
                                      minor_version_t minor = kAnyMinorVersion) const = 0;


    /**
     * @brief Offer an event or field in one or more event groups.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param event Event identifier.
     * @param eventgroups Event groups that contain the event.
     * @param is_field true when the event represents a field.
     */
    virtual void offer_event(service_t service, instance_t instance, event_t event,
                             const std::set<eventgroup_t>& eventgroups, bool is_field = false) = 0;

    /**
     * @brief Withdraw an offered event or field.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param event Event identifier.
     */
    virtual void stop_offer_event(service_t service, instance_t instance, event_t event) = 0;

    /**
     * @brief Declare interest in receiving an event.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param event Event identifier.
     * @param eventgroups Event groups that contain the event.
     * @note Call before subscribe() for each event the application wants to receive.
     */
    virtual void request_event(service_t service, instance_t instance, event_t event,
                               const std::set<eventgroup_t>& eventgroups) = 0;

    /**
     * @brief Remove a previously requested event.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param event Event identifier.
     */
    virtual void release_event(service_t service, instance_t instance, event_t event) = 0;

    /**
     * @brief Subscribe to an event group.
     *
     * Call request_event() for the desired events and request_service() for the
     * service before subscribing.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param eventgroup Event-group identifier.
     * @param major Major service version.
     * @param event_filter One requested event, or kAnyEvent for all requested events.
     */
    virtual void subscribe(service_t service, instance_t instance, eventgroup_t eventgroup,
                           major_version_t major = kDefaultMajorVersion,
                           event_t event_filter = kAnyEvent) = 0;

    /**
     * @brief Unsubscribe from an event group.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param eventgroup Event-group identifier.
     */
    virtual void unsubscribe(service_t service, instance_t instance, eventgroup_t eventgroup) = 0;

    /**
     * @brief Send a SOME/IP message.
     * @param message Message to send.
     * @note For a request with a zero session, the client and session are assigned
     *       internally. The const input object is not updated with those values.
     */
    virtual void send(const Message& message) = 0;

    /**
     * @brief Publish raw payload bytes for an offered event or field.
     *
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param event Event or field identifier.
     * @param payload Serialized payload bytes.
     * @param force Send unchanged field values when true.
     * @param flush true to flush queued data immediately; false to allow batching.
     */
    virtual void notify(service_t service, instance_t instance, event_t event,
                        const Payload& payload, bool force = false, bool flush = false) = 0;


    /**
     * @brief Register a callback for routing-daemon connection changes.
     *
     * @param handler Callback receiving true when registered and false when disconnected.
     *        Pass an empty handler to unregister the callback.
     * @note The state describes daemon registration, not service availability.
     */
    virtual void register_application_state_handler(ApplicationStateHandler handler) = 0;

    /**
     * @brief Register a callback for matching SOME/IP messages.
     * @param service Service identifier to match.
     * @param instance Instance identifier to match.
     * @param method Method or event identifier to match.
     * @param handler Callback invoked on a matching message.
     * @param is_provider true for provider-side requests; false for consumer-side responses and events.
     * @note Register handlers before start(); callbacks may run on worker threads.
     */
    virtual void register_message_handler(service_t service, instance_t instance, method_t method,
                                          MessageHandler handler, bool is_provider = false) = 0;
    /**
     * @brief Remove a message callback.
     * @param service Service identifier to match.
     * @param instance Instance identifier to match.
     * @param method Method or event identifier to match.
     * @param is_provider Provider-side or consumer-side handler direction.
     */
    virtual void unregister_message_handler(service_t service, instance_t instance, method_t method,
                                            bool is_provider = false) = 0;

    /**
     * @brief Register a callback for service availability transitions.
     * @param service Service identifier to match.
     * @param instance Instance identifier to match.
     * @param handler Callback receiving service, instance, and availability state.
     * @param major Major version to match, or kAnyMajorVersion.
     * @param minor Minor version to match, or kAnyMinorVersion.
     */
    virtual void register_availability_handler(
        service_t service, instance_t instance, AvailabilityHandler handler,
        major_version_t major = kAnyMajorVersion, minor_version_t minor = kAnyMinorVersion) = 0;

    /**
     * @brief Remove a service availability callback.
     * @param service Service identifier to match.
     * @param instance Instance identifier to match.
     * @param major Major version to match, or kAnyMajorVersion.
     * @param minor Minor version to match, or kAnyMinorVersion.
     */
    virtual void unregister_availability_handler(
        service_t service, instance_t instance, major_version_t major = kAnyMajorVersion,
        minor_version_t minor = kAnyMinorVersion) = 0;

    /**
     * @brief Register a provider-side subscription decision callback.
     * @param service Service identifier of the offered service.
     * @param instance Instance identifier of the offered instance.
     * @param eventgroup Event-group identifier.
     * @param handler Callback receiving client ID and subscribe/unsubscribe state;
     *        return true to accept a subscription.
     */
    virtual void register_subscription_handler(service_t service, instance_t instance,
                                               eventgroup_t eventgroup, SubscriptionHandler handler) = 0;

    /**
     * @brief Remove a provider-side subscription decision callback.
     * @param service Service identifier.
     * @param instance Instance identifier.
     * @param eventgroup Event-group identifier.
     */
    virtual void unregister_subscription_handler(service_t service, instance_t instance,
                                                 eventgroup_t eventgroup) = 0;
};

/**
 * @brief Process-wide factory for public LG SOME/IP objects.
 */
class Runtime {
public:
    /**
     * @brief Return the process-wide runtime instance.
     * @return The shared runtime factory.
     */
    static Runtime& instance();

    /**
     * @brief Destroy the runtime interface.
     */
    virtual ~Runtime();

    /**
     * @brief Create an application.
     * @param name Application name used for routing and configuration lookup.
     * @param config_path Configuration file path; an empty path uses the library default.
     * @return A new application handle.
     */
    virtual std::shared_ptr<Application> create_application(
        const std::string& name, const std::string& config_path = "") = 0;

protected:
    Runtime() = default;
};

} // namespace api
} // namespace lgsomeip

#endif // LG_SOMEIP_LGSOMEIP_API_H