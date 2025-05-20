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

#include "score/mw/com/test/twoface/src/twoface_service.h"
#include "score/mw/com/test/twoface/src/twoface_lola.h"

#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include "score/aracoreinitwrapper/aracoreinitializer.hpp"

#include <bmw/examples/exampleinterface/exampleinterface_skeleton.h>

#include <amp_stop_token.hpp>

#include <chrono>
#include <thread>
#include <utility>

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
                    using TwofaceService = examples::exampleinterface::skeleton::ExampleInterfaceSkeleton;

                    constexpr auto SEND_CYCLE_TIME{250ms};

                    int run_service()
                    {
                        aracoreinitwrapper::AraCoreInitializer _{};
                        amp::stop_source stop_service = amp::stop_source{};

                        const bool sig_term_handler_setup_success = bmw::mw::com::SetupStopTokenSigTermHandler(stop_service);
                        if (!sig_term_handler_setup_success)
                        {
                            std::cerr << "Unable to set signal handler, terminating!\n";
                            return -2;
                        }

                        auto instance_specifier_result = InstanceSpecifier::Create(kTwoFaceInstanceSpecifierString);
                        if (!instance_specifier_result.has_value())
                        {
                            std::cerr << "Could not create instance specifier, terminating\n";
                            return -1;
                        }
                        auto instance_specifier = std::move(instance_specifier_result).value();

                        auto service_result = TestDataSkeleton::Create(std::move(instance_specifier));

                        if (!service_result.has_value())
                        {
                            std::cerr << "Unable to construct TestDataSkeleton: " << service_result.error() << ", bailing!\n";
                            return -3;
                        }
                        auto &lola_service = service_result.value();

                        const auto offer_service_result = lola_service.OfferService();
                        if (!offer_service_result.has_value())
                        {
                            std::cerr << "Unable to offer service for TestDataSkeleton: " << offer_service_result.error() << ", bailing!\n";
                            return -4;
                        }

                        auto ara_com_service_token =
                            TwofaceService::Preconstruct(ara::core::InstanceSpecifier(ara::core::StringView(
                                                             "ServiceApp/ServiceApp_RootSwc/PPortAppExampleInterface")))
                                .Value();
                        TwofaceService ara_com_service{std::move(ara_com_service_token)};
                        ara_com_service.OfferService();

                        while (true)
                        {
                            ara_com_service.EventInteger.Send(17);
                            lola_service.test_event.Send(18);

                            if (!stop_service.stop_requested())
                            {
                                std::this_thread::sleep_for(SEND_CYCLE_TIME);
                            }
                            else
                            {
                                break;
                            }
                        }

                        lola_service.StopOfferService();
                        ara_com_service.StopOfferService();

                        return 0;
                    }

                } // namespace

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw

int main()
{
    return bmw::mw::com::test::run_service();
}
