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
 ********************************************************************************/
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROXY_CONTAINER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROXY_CONTAINER_H

#include "score/mw/com/types.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

namespace score::mw::com::test
{

template <typename Proxy>
class ProxyContainer
{
  public:
    ProxyContainer();

    bool CreateProxy(InstanceSpecifier instance_specifier, const std::vector<std::string_view>& enabled_method_names);

    Proxy& GetProxy()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_ != nullptr,
                                                    "Proxy was not successfully created! Cannot get it!");
        return *proxy_;
    }

  private:
    std::unique_ptr<typename Proxy::HandleType> handle_;
    std::mutex proxy_creation_mutex_;
    std::condition_variable proxy_creation_condition_variable_;

    std::unique_ptr<Proxy> proxy_;
};

template <typename Proxy>
ProxyContainer<Proxy>::ProxyContainer()
    : handle_{nullptr}, proxy_creation_mutex_{}, proxy_creation_condition_variable_{}, proxy_{nullptr}
{
}

template <typename Proxy>
bool ProxyContainer<Proxy>::CreateProxy(InstanceSpecifier instance_specifier,
                                        const std::vector<std::string_view>& enabled_method_names)
{
    bool callback_called{false};
    auto find_service_callback = [this, &callback_called](auto service_handle_container,
                                                          auto find_service_handle) mutable noexcept {
        std::cout << "Consumer: find service handler called" << std::endl;
        std::lock_guard lock{proxy_creation_mutex_};
        if (service_handle_container.size() != 1)
        {
            std::cerr << "Consumer: service handle container should contain 1 handle but contains: "
                      << service_handle_container.size() << std::endl;
            callback_called = true;
            proxy_creation_condition_variable_.notify_all();
            return;
        }
        handle_ = std::make_unique<typename Proxy::HandleType>(service_handle_container[0]);
        Proxy::StopFindService(find_service_handle);
        callback_called = true;
        proxy_creation_condition_variable_.notify_all();
    };

    auto start_find_service_result = Proxy::StartFindService(find_service_callback, instance_specifier);
    if (!start_find_service_result.has_value())
    {
        std::cerr << "Consumer: StartFindService() failed:" << start_find_service_result.error() << std::endl;
        return false;
    }
    std::cout << "Consumer: StartFindService called" << std::endl;

    // Wait for the find service handler to be called (with timeout to prevent indefinite blocking)
    constexpr auto kServiceDiscoveryTimeout = std::chrono::seconds(10);
    std::unique_lock proxy_creation_lock{proxy_creation_mutex_};
    const bool service_found =
        proxy_creation_condition_variable_.wait_for(proxy_creation_lock, kServiceDiscoveryTimeout, [&callback_called] {
            return callback_called;
        });
    if (!service_found || handle_ == nullptr)
    {
        std::cerr << "Consumer: StartFindService() failed to get handle" << std::endl;
        return false;
    }

    auto proxy_result = Proxy::Create(*handle_, enabled_method_names);
    proxy_creation_lock.unlock();
    if (!proxy_result.has_value())
    {
        std::cerr << "Consumer: Unable to construct proxy: " << proxy_result.error() << ", bailing!\n";
        return false;
    }
    proxy_ = std::make_unique<Proxy>(std::move(proxy_result).value());

    std::cout << "Consumer: Proxy created successfully" << std::endl;
    return true;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROXY_CONTAINER_H
