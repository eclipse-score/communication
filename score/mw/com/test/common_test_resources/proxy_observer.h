/*******************************************************************************
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
 *******************************************************************************/

#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/types.h"

#include <score/optional.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::test
{

namespace
{
template <typename Proxy>
class ProxyObserver
{
  public:
    ProxyObserver(std::string_view instance_Specifier)
        : instance_specifier_result_{InstanceSpecifier::Create(std::string{instance_Specifier})}
    {
        if (!instance_specifier_result_.has_value())
        {
            std::cerr << "Unable to create instance specifier\n";
        }
    }

    ~ProxyObserver()
    {
        if (handle_.has_value())
        {
            Proxy::StopFindService(handle_.value());
        }
    }

    Result<FindServiceHandle> StartServiceDiscovery(std::size_t required_number_of_services,
                                                    const score::cpp::stop_token& stop_token) noexcept
    {
        auto callback = [this, required_number_of_services, &stop_token](auto service_handle_container, auto) noexcept {
            std::cout << "handler called" << std::endl;
            for (auto& handle : service_handle_container)
            {
                auto lola_proxy_result = Proxy::Create(handle);
                if (lola_proxy_result.has_value())
                {
                    std::cout << "successfully created lola proxy" << std::endl;
                    proxies_.push_back(std::move(lola_proxy_result.value()));
                }
                else
                {
                    std::cerr << "unable to create lola proxy:" << lola_proxy_result.error().Message() << std::endl;
                }
            }
            if (proxies_.size() == required_number_of_services)
            {
                std::cout << "Requested number of proxies created" << std::endl;
                promise_.SetValue();
            }
            stop_token.stop_requested();
        };

        handle_ = Proxy::StartFindService(callback, instance_specifier_result_.value());
        return handle_;
    }

    std::int32_t CheckProxyCreation(const score::cpp::stop_token& stop_token) noexcept
    {
        auto future = promise_.GetInterruptibleFuture().value();
        auto future_status = future.Wait(stop_token);
        if (!future_status.has_value())
        {
            std::cerr << "Could not find the requested number of services" << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

  private:
    score::Result<score::mw::com::impl::InstanceSpecifier> instance_specifier_result_;
    std::vector<Proxy> proxies_{};
    concurrency::InterruptiblePromise<void> promise_{};
    // handle has deleted default constructor
    Result<FindServiceHandle> handle_ = MakeUnexpected(score::mw::com::impl::ComErrc::kUnsetFailure);
};

}  // namespace

}  // namespace score::mw::com::test
