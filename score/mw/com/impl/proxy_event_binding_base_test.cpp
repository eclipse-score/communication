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
#include "score/mw/com/impl/proxy_event_binding_base.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace score::mw::com::impl
{
namespace
{

class DummyProxyEventBinding final : public ProxyEventBindingBase
{
  public:
    ResultBlank Subscribe(std::size_t) noexcept override
    {
        return {};
    }
    SubscriptionState GetSubscriptionState() const noexcept override
    {
        return SubscriptionState::kSubscribed;
    }
    void Unsubscribe() noexcept override {}
    ResultBlank SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler>) noexcept override
    {
        return {};
    }
    ResultBlank UnsetReceiveHandler() noexcept override
    {
        return {};
    }
    Result<std::size_t> GetNumNewSamplesAvailable() const noexcept override
    {
        return {};
    }
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override
    {
        return {};
    }
    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kFake;
    }
    void NotifyServiceInstanceChangedAvailability(bool, pid_t) noexcept override {}
};

TEST(ProxyEventBindingBaseTest, ProxyEventBindingBaseShouldNotBeCopyable)
{
    static_assert(!std::is_copy_constructible<DummyProxyEventBinding>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<DummyProxyEventBinding>::value, "Is wrongly copyable");
}

TEST(ProxyEventBindingBaseTest, ProxyEventBindingBaseShouldNotBeMoveable)
{
    static_assert(!std::is_move_constructible<DummyProxyEventBinding>::value, "Is wrongly moveable");
    static_assert(!std::is_move_assignable<DummyProxyEventBinding>::value, "Is wrongly moveable");
}

}  // namespace
}  // namespace score::mw::com::impl
