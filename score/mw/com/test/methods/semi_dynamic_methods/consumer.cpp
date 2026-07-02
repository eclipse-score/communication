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
#include "score/mw/com/test/methods/semi_dynamic_methods/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/methods/semi_dynamic_methods/runtime_sized_array.h"
#include "score/mw/com/test/methods/semi_dynamic_methods/semi_dynamic_method_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <unistd.h>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

using ArrayValueType = SemiDynamicMethodProxy::ArrayValueType;

const std::string kInterprocessNotificationShmPath{"/semi_dynamic_methods_test_interprocess_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/methods/semi_dynamic_methods/TestMethods"}).value();

constexpr std::size_t kNumberOfRuntimeSizedArrayElements = 10U;
constexpr std::size_t kNumberOfMethodCalls = 10U;

// Additional required shared memory is the number of elements in the runtime sized array multiplied by the size of each
// element (ArrayValueType). The factor of 2 is because we allocate once for the in arg and once for the return value.
constexpr std::size_t kAdditionalShmSizeBytes = 2 * kNumberOfRuntimeSizedArrayElements * sizeof(ArrayValueType);

template <typename Proxy>
class SemiDynamicProxyContainer
{
  public:
    void CreateProxy(InstanceSpecifier instance_specifier,
                     std::size_t additional_shm_size_bytes,
                     const std::string& failure_message_prefix);

    Proxy& GetProxy()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_ != nullptr,
                                                    "Proxy was not successfully created! Cannot get it!");
        return *proxy_;
    }

  private:
    std::unique_ptr<typename Proxy::HandleType> handle_{nullptr};
    std::mutex proxy_creation_mutex_{};
    std::condition_variable proxy_creation_condition_variable_{};

    std::unique_ptr<Proxy> proxy_{nullptr};
};

template <typename Proxy>
void SemiDynamicProxyContainer<Proxy>::CreateProxy(InstanceSpecifier instance_specifier,
                                                   std::size_t additional_shm_size_bytes,
                                                   const std::string& failure_message_prefix)
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
        score::cpp::ignore = Proxy::StopFindService(find_service_handle);
        callback_called = true;
        proxy_creation_condition_variable_.notify_all();
    };

    auto start_find_service_result = Proxy::StartFindService(find_service_callback, instance_specifier);
    if (!start_find_service_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: StartFindService() failed: ", start_find_service_result.error());
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
        FailTest(failure_message_prefix, " Consumer: StartFindService() failed to get handle");
    }

    auto proxy_result = impl::ProxyWithSemiDynamicMethodFactory<Proxy>::Create(*handle_, additional_shm_size_bytes);
    proxy_creation_lock.unlock();
    if (!proxy_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: Unable to construct proxy: ", proxy_result.error());
    }
    proxy_ = std::make_unique<Proxy>(std::move(proxy_result).value());

    std::cout << "Consumer: Proxy created successfully" << std::endl;
}

void CallMethodZeroCopyWithAllocation(SemiDynamicMethodProxy& proxy)
{
    auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
    if (!allocated_args_result.has_value())
    {
        FailTest("Consumer: Could not allocate method args: ", allocated_args_result.error());
    }
    auto allocated_args = std::move(allocated_args_result).value();
    auto& [runtime_sized_array_in_arg_ptr] = allocated_args;

    runtime_sized_array_in_arg_ptr->ReserveOnce(kNumberOfRuntimeSizedArrayElements);
    for (std::size_t i = 0U; i < kNumberOfRuntimeSizedArrayElements; ++i)
    {
        runtime_sized_array_in_arg_ptr->emplace_back(static_cast<ArrayValueType>(i));
    }

    auto method_return_result = proxy.with_in_args_and_return(std::move(runtime_sized_array_in_arg_ptr));
    if (!(method_return_result.has_value()))
    {
        FailTest("Consumer: zero-copy call failed: ", method_return_result.error());
    }
    const auto& actual_return_value = *(method_return_result.value());
    for (std::size_t i = 0U; i < kNumberOfRuntimeSizedArrayElements; ++i)
    {
        const auto expected_value = static_cast<ArrayValueType>(i * 2U);
        if (actual_return_value.at(i) != expected_value)
        {
            FailTest("Consumer: zero-copy expected ",
                     static_cast<int>(expected_value),
                     " but got ",
                     static_cast<int>(actual_return_value.at(i)));
        }
    }
}

void CallMethodZeroCopyWithoutAllocation(SemiDynamicMethodProxy& proxy, const std::size_t iteration)
{
    auto allocated_args_result = proxy.with_in_args_and_return.Allocate();
    if (!allocated_args_result.has_value())
    {
        FailTest("Consumer: Could not allocate method args: ", allocated_args_result.error());
    }
    auto allocated_args = std::move(allocated_args_result).value();
    auto& [runtime_sized_array_in_arg_ptr] = allocated_args;

    // The in arg was already reserved in the previous call, so we can just fill it with new values without reserving
    // again.
    for (std::size_t i = 0U; i < kNumberOfRuntimeSizedArrayElements; ++i)
    {
        runtime_sized_array_in_arg_ptr->at(i) = static_cast<std::uint32_t>(i * iteration);
    }

    auto method_return_result = proxy.with_in_args_and_return(std::move(runtime_sized_array_in_arg_ptr));
    if (!(method_return_result.has_value()))
    {
        FailTest("Consumer: zero-copy call failed: ", method_return_result.error());
    }
    const auto& actual_return_value = *(method_return_result.value());
    for (std::size_t i = 0U; i < kNumberOfRuntimeSizedArrayElements; ++i)
    {
        const auto expected_value = static_cast<ArrayValueType>(i * 2U * iteration);
        if (actual_return_value.at(i) != expected_value)
        {
            FailTest("Consumer: zero-copy expected ",
                     static_cast<int>(expected_value),
                     " but got ",
                     static_cast<int>(actual_return_value.at(i)));
        }
        std::cout << "Consumer: zero-copy returned correct result for iteration " << iteration << ": "
                  << static_cast<int>(actual_return_value.at(i)) << std::endl;
    }
}

}  // namespace

void run_consumer()
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("Methods semi_dynamic_methods_test consumer failed: Could not create ProcessSynchronizer");
    }

    // Set an exit function to notify the provider in case of failure in calls to FailTest, so that it does not wait
    // indefinitely for the consumer to finish. Will also be called when the guard goes out of scope at the end of this
    // function.
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    SemiDynamicProxyContainer<SemiDynamicMethodProxy> proxy_container{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifier, kAdditionalShmSizeBytes, "semi_dynamic_methods_test");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Call zero copy method - cannot use copy api since RuntimeSizedArray is not copyable
    std::cout << "\nConsumer: Step 2 - Call zero copy method" << std::endl;
    CallMethodZeroCopyWithAllocation(proxy);

    // Step 3. Call zero copy method again (should not reallocate - if it does, it will crash since the memory resource
    // will not have any available memory)
    static_assert(kNumberOfMethodCalls > 1,
                  "kNumberOfMethodCalls must be greater than 1 so that the method is re-called");
    for (auto i = 0U; i < kNumberOfMethodCalls - 1U; ++i)
    {
        std::cout << "Consumer: Step 3 - Call zero copy method again, iteration " << i << std::endl;
        CallMethodZeroCopyWithoutAllocation(proxy, i);
    }

    // Step 4. Notify provider that consumer is done
    std::cout << "\nConsumer: Step 4 - Notify provider that consumer is done" << std::endl;
    process_synchronizer_result->Notify();
}

}  // namespace score::mw::com::test
