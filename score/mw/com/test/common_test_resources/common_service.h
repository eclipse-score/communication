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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMON_SERVICE_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMON_SERVICE_H

#include "score/mw/com/test/common_test_resources/test_error_domain.h"
#include "score/mw/com/types.h"

#include <chrono>
#include <iostream>

namespace bmw
{
    namespace mw
    {
        namespace com
        {
            namespace test
            {

                template <typename T>
                class Service
                {
                public:
                    ~Service() { lola_service_.StopOfferService(); }

                    Service(Service &&other) noexcept : lola_service_(std::move(other.lola_service_)) {}

                    static Result<Service> Create(amp::string_view instance_specifier_string)
                    {
                        auto instance_specifier_result = InstanceSpecifier::Create(instance_specifier_string);
                        if (!instance_specifier_result.has_value())
                        {
                            return MakeUnexpected(TestErrorCode::kCreateInstanceSpecifierFailed,
                                                  "Unable to create instance specifier, terminating");
                        }
                        auto instance_specifier = std::move(instance_specifier_result).value();

                        auto service_result = T::Create(std::move(instance_specifier));
                        if (!service_result.has_value())
                        {
                            return MakeUnexpected(TestErrorCode::kCreateSkeletonFailed,
                                                  ("Unable to construct TestDataSkeleton: " +
                                                   std::string(service_result.error().Message().data()) + ", bailing!")
                                                      .data());
                        }

                        return bmw::Result<Service>(Service(std::move(service_result.value())));
                    }

                    [[nodiscard]] ResultBlank OfferService(const std::int32_t test_value)
                    {
                        std::cout << "Start offering service, with value " << test_value << "\n";
                        lola_service_.test_field.Update(test_value);
                        const auto result = lola_service_.OfferService();
                        if (!result.has_value())
                        {
                            std::cerr << "Failed to offer serivce: " << result.error().Message().data();
                        }
                        return result;
                    }

                private:
                    Service(T lola_service) : lola_service_{std::move(lola_service)} {}

                    T lola_service_;
                };

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw
#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMON_SERVICE_H
