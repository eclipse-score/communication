/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "score/mw/com/test/partial_restart/provider_restart_max_subscribers/consumer.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/partial_restart/consumer_handle_notification_data.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"
#include "score/mw/com/types.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

namespace bmw::mw::com::test
{

    namespace
    {

        const auto kProxyInstanceSpecifier = InstanceSpecifier::Create("partial_restart/small_but_great0").value();
        const std::chrono::seconds kMaxHandleNotificationWaitTime{15U};

        void NotifyThatHandleReceived(const ServiceHandleContainer<TestServiceProxy::HandleType> service_handle_container,
                                      const FindServiceHandle find_service_handle,
                                      HandleNotificationData &handle_notification_data,
                                      CheckPointControl &check_point_control) noexcept
        {
            std::cerr << "Consumer: find service handler called" << std::endl;
            if (service_handle_container.size() != 1)
            {
                std::cerr << "Consumer: Error - StartFindService() is expected to find 1 service instance but found: "
                          << service_handle_container.size() << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            std::lock_guard lock{handle_notification_data.mutex};
            handle_notification_data.handle = std::make_unique<TestServiceProxy::HandleType>(service_handle_container[0]);
            handle_notification_data.condition_variable.notify_all();
            std::cerr << "Consumer: FindServiceHandler handler done - found one service instance." << std::endl;

            TestServiceProxy::StopFindService(find_service_handle);
        }

    } // namespace

    ConsumerActions::ConsumerActions(CheckPointControl &check_point_control,
                                     amp::stop_token test_stop_token,
                                     int argc,
                                     const char **argv,
                                     ConsumerParameters consumer_parameters) noexcept
        : check_point_control_{check_point_control},
          test_stop_token_{std::move(test_stop_token)},
          consumer_parameters_{consumer_parameters},
          proxies_{}
    {
        // Initialize mw::com runtime explicitly, if we were called with cmd-line args from main/parent
        if (argc > 0 && argv != nullptr)
        {
            std::cerr
                << "Consumer: Initializing LoLa/mw::com runtime from cmd-line args handed over by parent/controller ..."
                << std::endl;
            mw::com::runtime::InitializeRuntime(argc, argv);
            std::cerr << "Consumer: Initializing LoLa/mw::com runtime done." << std::endl;
        }
    }

    void ConsumerActions::DoConsumerActions() noexcept
    {
        DoConsumerActionsBeforeRestart();

        // ********************************************************************************
        // Step (C.9) - Notify to Controller, that checkpoint (1) has been reached
        // ********************************************************************************
        std::cout << "Consumer Step (C.9): Notify controller that checkpoint 1 has been reached" << std::endl;
        check_point_control_.CheckPointReached(1);

        // ********************************************************************************
        // Step (C.10) - Wait for proceed trigger from Controller
        // ********************************************************************************
        std::cout << "Consumer Step (C.10): Wait for proceed trigger from Controller" << std::endl;
        auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control_, test_stop_token_);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            std::cerr << "Consumer Step (C.10): Expected to get notification to continue to next checkpoint but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control_.ErrorOccurred();
            return;
        }

        DoConsumerActionsAfterRestart();

        // ********************************************************************************
        // Step (C.19) - Notify to Controller, that checkpoint (2) has been reached
        // ********************************************************************************
        std::cout << "Consumer Step (C.19): Notify controller that checkpoint 2 has been reached" << std::endl;
        check_point_control_.CheckPointReached(2);

        // ********************************************************************************
        // Step (C.20) - Wait for proceed trigger from Controller to indicate that consumer can finish.
        // ********************************************************************************
        std::cout << "Consumer Step (C.20): Wait for proceed trigger from Controller to indicate that consumer can finish."
                  << std::endl;
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control_, test_stop_token_);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            std::cerr << "Consumer Step (C.20): Expected to get notification to finish but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control_.ErrorOccurred();
            return;
        }

        std::cout << "Consumer: Finishing actions!" << std::endl;
    }

    void ConsumerActions::DoConsumerActionsBeforeRestart() noexcept
    {
        // ********************************************************************************
        // Step (C.1) - Start an async FindService Search
        // ********************************************************************************
        std::cout << "Consumer Step (C.1): Call StartFindService" << std::endl;
        HandleNotificationData handle_notification_data{};
        auto find_service_callback = [this, &handle_notification_data](auto service_handle_container,
                                                                       auto find_service_handle) noexcept
        {
            NotifyThatHandleReceived(
                service_handle_container, find_service_handle, handle_notification_data, check_point_control_);
        };

        auto find_service_handle_result = StartFindService<TestServiceProxy>(
            "Consumer Step (C.1)", find_service_callback, kProxyInstanceSpecifier, check_point_control_);
        if (!find_service_handle_result.has_value())
        {
            return;
        }

        // ********************************************************************************
        // Step (C.2) - Wait for FindServiceHandler to be called. Call StopFindService in handler
        // ********************************************************************************
        std::cout << "Consumer Step (C.2): Wait for FindServiceHandler to be called" << std::endl;
        std::unique_lock lock{handle_notification_data.mutex};
        const auto wait_result = handle_notification_data.condition_variable.wait_for(
            lock, kMaxHandleNotificationWaitTime, [&handle_notification_data]
            { return handle_notification_data.handle != nullptr; });
        lock.unlock();
        if (!wait_result)
        {
            std::cerr << "Consumer: Did not receive handle in time!" << std::endl;
            check_point_control_.ErrorOccurred();
            return;
        }

        // ********************************************************************************
        // Step (C.3) - Create n Proxies for found service and store in vector - where n is the value of
        //            max_subscribers in the configuration
        // ********************************************************************************
        std::cout << "Consumer Step (C.3): Create n Proxies for found service and store in vector" << std::endl;
        for (std::size_t i = 0; i < consumer_parameters_.max_number_subscribers; ++i)
        {
            auto proxy_wrapper_result =
                CreateProxy<TestServiceProxy>("Consumer", *handle_notification_data.handle, check_point_control_);
            if (!(proxy_wrapper_result.has_value()))
            {
                return;
            }
            proxies_.push_back(std::move(proxy_wrapper_result).value());
        }
        std::cerr << "Consumer Step (C.3): Created N proxies" << std::endl;

        // ********************************************************************************
        // Step (C.4) - Create single additional Proxy for found service
        // ********************************************************************************
        std::cout << "Consumer Step (C.4): Create single additional Proxy for found service" << std::endl;
        auto additional_proxy_wrapper_result =
            CreateProxy<TestServiceProxy>("Consumer", *handle_notification_data.handle, check_point_control_);
        if (!(additional_proxy_wrapper_result.has_value()))
        {
            return;
        }
        std::cerr << "Consumer Step (C.4): Created additional proxy" << std::endl;

        for (std::size_t i = 0; i < proxies_.size(); ++i)
        {
            auto &proxy = proxies_[i];
            std::cout << "Consumer: Subscribing to proxy #" << i << std::endl;

            // ********************************************************************************
            // Step (C.5) - Subscribe to Event
            // ********************************************************************************
            std::cout << "Consumer Step (C.5): Subscribe to Event" << std::endl;
            const std::size_t max_sample_count{1U};
            auto subscribe_result = SubscribeProxyEvent<decltype(proxy.simple_event_)>(
                "Consumer Step (C.5)", proxy.simple_event_, max_sample_count, check_point_control_);
            if (!(subscribe_result.has_value()))
            {
                return;
            }

            // ********************************************************************************
            // Step (C.6) - Check that subscription state is Subscribed
            // ********************************************************************************
            std::cout << "Consumer Step (C.6): Check that subscription state is Subscribed" << std::endl;
            const auto subscription_state = proxy.simple_event_.GetSubscriptionState();
            if (subscription_state != SubscriptionState::kSubscribed)
            {
                std::cerr << "Consumer Step (C.6): ProxyEvent is not subscribed!" << std::endl;
                check_point_control_.ErrorOccurred();
                return;
            }
        }

        // ********************************************************************************
        // Step (C.7) - For single additional Proxy: Subscribe to SkeletonEvent and assert that error is returned
        //              indicating that we couldn't subscribe
        // ********************************************************************************
        std::cout << "Consumer Step (C.7): Subscribe to SkeletonEvent with single additional Proxy (expecting failure)"
                  << std::endl;
        const std::size_t max_sample_count{1U};
        const auto subscription_result = additional_proxy_wrapper_result.value().simple_event_.Subscribe(max_sample_count);
        if (subscription_result.has_value())
        {
            std::cerr << "Consumer: ProxyEvent was able to subscribe even though max subscribers has already been reached!"
                      << std::endl;
            check_point_control_.ErrorOccurred();
            return;
        }

        if (!consumer_parameters_.is_proxy_connected_during_restart)
        {
            // ********************************************************************************
            // Step (C.8) - Destroy all proxies
            // ********************************************************************************
            std::cout << "Consumer Step (C.8): Destroy all existing proxies" << std::endl;
            proxies_.clear();
        }
    }

    void ConsumerActions::DoConsumerActionsAfterRestart() noexcept
    {
        // ********************************************************************************
        // Step (C.11) - Start an async FindService Search
        // ********************************************************************************
        std::cout << "Consumer Step (C.11): Call StartFindService" << std::endl;
        HandleNotificationData handle_notification_data{};
        auto find_service_callback = [this, &handle_notification_data](auto service_handle_container,
                                                                       auto find_service_handle) noexcept
        {
            NotifyThatHandleReceived(
                service_handle_container, find_service_handle, handle_notification_data, check_point_control_);
        };

        auto find_service_handle_result = StartFindService<TestServiceProxy>(
            "Consumer", find_service_callback, kProxyInstanceSpecifier, check_point_control_);
        if (!find_service_handle_result.has_value())
        {
            return;
        }
        std::cerr << "Consumer Step (C.11): StartFindServiceHandler succeeded" << std::endl;

        // ********************************************************************************
        // Step (C.12) - Wait for FindServiceHandler to be called. Call StopFindService in handler
        // ********************************************************************************
        std::cout << "Consumer Step (C.12): Wait for FindServiceHandler to be called." << std::endl;
        std::unique_lock lock{handle_notification_data.mutex};
        const auto wait_result = handle_notification_data.condition_variable.wait_for(
            lock, kMaxHandleNotificationWaitTime, [&handle_notification_data]
            { return handle_notification_data.handle != nullptr; });
        lock.unlock();
        if (!wait_result)
        {
            std::cerr << "Consumer: Did not receive handle in time!" << std::endl;
            check_point_control_.ErrorOccurred();
            return;
        }
        if (consumer_parameters_.is_proxy_connected_during_restart)
        {
            for (std::size_t i = 0; i < proxies_.size(); ++i)
            {
                std::cerr << "Consumer: Checking that existing proxy re-subscribes: " << i << std::endl;

                // ********************************************************************************
                // Step (C.13) - Wait for the subscription state to be Subscribed
                // ********************************************************************************
                std::cout << "Consumer Step (C.13): Wait for the subscription state to be Subscribed" << std::endl;
                const std::size_t max_retries{30U};
                const std::chrono::milliseconds retry_sleep_time{20U};
                std::size_t retry_count{0U};
                while ((proxies_[i].simple_event_.GetSubscriptionState() != SubscriptionState::kSubscribed))
                {
                    std::this_thread::sleep_for(retry_sleep_time);
                    if (retry_count > max_retries)
                    {
                        std::cerr << "Consumer Step (C.13): Max number of retries exceeded while waiting for ProxyEvent to "
                                     "resubscribe."
                                  << std::endl;
                        check_point_control_.ErrorOccurred();
                        return;
                    }
                    retry_count++;
                }
            }
            std::cerr << "Consumer: All existing proxies have re-subscribed" << std::endl;
        }
        else
        {
            // ********************************************************************************
            // Step (C.14) - Create n Proxies for found service and store in vector - where n is the value of
            //               max_subscribers in the configuration
            // ********************************************************************************
            std::cout << "Consumer Step (C.14): Create n Proxies for found service and store in vector" << std::endl;
            for (std::size_t i = 0; i < consumer_parameters_.max_number_subscribers; ++i)
            {
                auto proxy_wrapper_result =
                    CreateProxy<TestServiceProxy>("Consumer", *handle_notification_data.handle, check_point_control_);
                if (!(proxy_wrapper_result.has_value()))
                {
                    return;
                }
                proxies_.push_back(std::move(proxy_wrapper_result).value());
            }
            std::cerr << "Consumer Step (C.14): Created N proxies" << std::endl;

            for (std::size_t i = 0; i < proxies_.size(); ++i)
            {
                auto &proxy = proxies_[i];

                std::cerr << "Consumer: Checking that existing proxy re-subscribes: " << i << std::endl;
                // ********************************************************************************
                // Step (C.15) - Subscribe to Event
                // ********************************************************************************
                std::cout << "Consumer Step (C.15): Subscribe to Event" << std::endl;
                const std::size_t max_sample_count{1U};
                auto subscribe_result = SubscribeProxyEvent<decltype(proxy.simple_event_)>(
                    "Consumer Step (C.15)", proxy.simple_event_, max_sample_count, check_point_control_);
                if (!(subscribe_result.has_value()))
                {
                    return;
                }

                // ********************************************************************************
                // Step (C.16) - Check that subscription state is Subscribed
                // ********************************************************************************
                std::cout << "Consumer Step (C.16): Check that subscription state is Subscribed" << std::endl;
                const auto subscription_state = proxy.simple_event_.GetSubscriptionState();
                if (subscription_state != SubscriptionState::kSubscribed)
                {
                    std::cerr << "Consumer: ProxyEvent is not subscribed!" << std::endl;
                    check_point_control_.ErrorOccurred();
                    return;
                }
            }
        }

        // ********************************************************************************
        // Step (C.17) - Create single additional Proxy for found service
        // ********************************************************************************
        auto additional_proxy_wrapper_result =
            CreateProxy<TestServiceProxy>("Consumer", *handle_notification_data.handle, check_point_control_);
        if (!(additional_proxy_wrapper_result.has_value()))
        {
            return;
        }
        std::cerr << "Consumer: Created additional proxy" << std::endl;

        // ********************************************************************************
        // Step (C.18) - For single additional Proxy: Subscribe to SkeletonEvent and assert that error is returned
        //               indicating that we couldn't subscribe
        // ********************************************************************************
        const std::size_t max_sample_count{1U};
        const auto subscription_result = additional_proxy_wrapper_result.value().simple_event_.Subscribe(max_sample_count);
        if (subscription_result.has_value())
        {
            std::cerr << "Consumer: ProxyEvent was able to subscribe even though max subscribers has already been reached!"
                      << std::endl;
            check_point_control_.ErrorOccurred();
            return;
        }
    }

} // namespace bmw::mw::com::test
