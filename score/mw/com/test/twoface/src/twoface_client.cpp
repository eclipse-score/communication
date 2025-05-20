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

#include "score/mw/com/test/twoface/src/twoface_client.h"
#include "score/mw/com/test/twoface/src/twoface_lola.h"

#include "score/aracoreinitwrapper/aracoreinitializer.hpp"

#include "score/mw/com/types.h"

#include <bmw/examples/exampleinterface/exampleinterface_proxy.h>

#include <chrono>
#include <future>
#include <thread>

namespace bmw
{
    namespace mw
    {
        namespace com
        {
            namespace test
            {

                namespace
                {

                    using namespace std::chrono_literals;

                    constexpr auto ARA_COM_FIND_SERVICE_TIMEOUT{3s};
                    constexpr auto RETRY_BACKOFF_TIME{350ms};
                    constexpr auto NUM_RETRIES{5U};
                    constexpr auto MAX_NUM_SAMPLES{1U};

                    int run_client()
                    {
                        using bmw::examples::exampleinterface::proxy::ExampleInterfaceProxy;
                        using bmw::mw::com::test::TestDataProxy;

                        bmw::aracoreinitwrapper::AraCoreInitializer _{};

                        std::promise<ExampleInterfaceProxy::HandleType> handle{};
                        ara::com::FindServiceHandle find_handle = ExampleInterfaceProxy::StartFindService(
                            [&find_handle, &handle](auto find_service_container)
                            {
                                if (!find_service_container.empty())
                                {
                                    handle.set_value(find_service_container[0]);
                                    ExampleInterfaceProxy::StopFindService(find_handle);
                                }
                            },
                            ara::core::InstanceSpecifier{ara::core::StringView{"ClientApp/ClientApp_RootSwc/RPortAppExampleInterface"}});

                        std::future<ExampleInterfaceProxy::HandleType> handle_fut = handle.get_future();
                        if (handle_fut.wait_for(ARA_COM_FIND_SERVICE_TIMEOUT) != std::future_status::ready)
                        {
                            std::cerr << "Unable to find ara service, terminating\n";
                            return -1;
                        }
                        ExampleInterfaceProxy ara_proxy{ExampleInterfaceProxy::Preconstruct(handle_fut.get()).Value()};

                        auto instance_specifier_result = InstanceSpecifier::Create(kTwoFaceInstanceSpecifierString);
                        if (!instance_specifier_result.has_value())
                        {
                            std::cerr << "Could not create instance specifier, terminating\n";
                            return -1;
                        }
                        auto instance_specifier = std::move(instance_specifier_result).value();

                        auto lola_proxy_handles_result = TestDataProxy::FindService(std::move(instance_specifier));
                        if (!lola_proxy_handles_result.has_value())
                        {
                            std::cerr << "FindService returned an error, terminating!\n";
                            return -1;
                        }

                        const auto lola_proxy_handles = std::move(lola_proxy_handles_result.value());
                        if (lola_proxy_handles.empty())
                        {
                            std::cerr << "Unable to find lola service, terminating\n";
                            return -1;
                        }

                        auto lola_proxy_result = TestDataProxy::Create(lola_proxy_handles[0]);
                        if (!lola_proxy_result.has_value())
                        {
                            std::cerr << "Unable to create lola proxy, terminating\n";
                            return -1;
                        }
                        auto &lola_proxy = lola_proxy_result.value();

                        bool lola_received{false};
                        bool ara_com_received{false};

                        lola_proxy.test_event.Subscribe(MAX_NUM_SAMPLES);
                        ara_proxy.EventInteger.Subscribe(MAX_NUM_SAMPLES);

                        for (auto retries = NUM_RETRIES; retries > 0; --retries)
                        {
                            lola_proxy.test_event.GetNewSamples([&](const auto &) noexcept
                                                                { lola_received = true; }, MAX_NUM_SAMPLES);
                            ara_proxy.EventInteger.GetNewSamples([&](const auto &) noexcept
                                                                 { ara_com_received = true; }, MAX_NUM_SAMPLES);

                            if (!ara_com_received || !lola_received)
                            {
                                std::this_thread::sleep_for(RETRY_BACKOFF_TIME);
                            }
                            else
                            {
                                break;
                            }
                        }

                        lola_proxy.test_event.Unsubscribe();
                        ara_proxy.EventInteger.Unsubscribe();

                        if (!lola_received)
                        {
                            std::cerr << "Lola didn't receive a sample!\n";
                        }

                        if (!ara_com_received)
                        {
                            std::cerr << "ara::com didn't receive a sample!\n";
                        }

                        return ara_com_received && lola_received ? 0 : -3;
                    }

                } // namespace

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw

int main()
{
    return bmw::mw::com::test::run_client();
}
