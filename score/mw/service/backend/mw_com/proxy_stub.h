/*********************************************************************************
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

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_PROXY_STUB_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_PROXY_STUB_H

#include "score/result/result.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>

#include <gmock/gmock.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace score::mw::service::backend::mw_com
{

using ::testing::_;
using ::testing::Return;

class MockHandle
{
  public:
    std::uint32_t instance_id;
};

template <typename MwComProxy>
class FakeProxyMockGuard
{
  public:
    FakeProxyMockGuard()
    {
        MwComProxy::SetUpMockInstance();
    }

    ~FakeProxyMockGuard()
    {
        MwComProxy::DestroyMockInstance();
    }
};

/// @brief mw::com proxy class which only offers asynchronous APIs to be used for event-driven service discovery process
///
/// Note. this is a temporary class which is required until mw::com provides its own mocking solution.
class FakeProxy
{
  public:
    using HandleType = MockHandle;
    using ProxyHandles = score::mw::com::ServiceHandleContainer<HandleType>;

    class Mock
    {
      public:
        Mock() {}

        MOCK_METHOD(score::mw::com::FindServiceHandle,
                    StartFindService,
                    (score::mw::com::FindServiceHandler<HandleType>, score::mw::com::InstanceSpecifier),
                    ());
        MOCK_METHOD(score::Result<void>, StopFindService, (score::mw::com::FindServiceHandle), ());
    };

    static Result<FakeProxy> Create(const HandleType&) noexcept
    {
        return gFakeProxyCreationResult;
    }

    static void InjectCreateError(Result<FakeProxy> fake_proxy_creation_result)
    {
        gFakeProxyCreationResult = std::move(fake_proxy_creation_result);
    }

    static Result<score::mw::com::FindServiceHandle> StartFindService(
        score::mw::com::FindServiceHandler<HandleType> handler,
        score::mw::com::InstanceSpecifier instance_specifier)
    {
        return GetMockInstance().StartFindService(std::move(handler), std::move(instance_specifier));
    }

    static score::Result<void> StopFindService(score::mw::com::FindServiceHandle handle)
    {
        return GetMockInstance().StopFindService(handle);
    }

    static score::mw::com::FindServiceHandle CreateFindServiceHandle(const std::size_t uid)
    {
        return score::mw::com::impl::make_FindServiceHandle(uid);
    }

    ~FakeProxy() noexcept = default;

    static Mock& GetMockInstance()
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(gMockInstance != nullptr);
        return *gMockInstance;
    }

    static void SetUpMockInstance()
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(gMockInstance == nullptr);
        gMockInstance = std::make_unique<Mock>();
    }

    static void DestroyMockInstance()
    {
        gMockInstance.reset();
    }

  private:
    FakeProxy() {}
    static std::unique_ptr<Mock> gMockInstance;
    static Result<FakeProxy> gFakeProxyCreationResult;
};

bool operator==(const MockHandle& lhs, const MockHandle& rhs) noexcept;
bool operator<(const MockHandle& lhs, const MockHandle& rhs) noexcept;

}  // namespace score::mw::service::backend::mw_com

namespace std
{
template <>
class hash<score::mw::service::backend::mw_com::MockHandle>
{
  public:
    std::size_t operator()(const score::mw::service::backend::mw_com::MockHandle& handle_type) const noexcept
    {
        return handle_type.instance_id;
    }
};

}  // namespace std

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROXY_STUB_H
