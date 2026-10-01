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

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_PROXY_HOLDER_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_PROXY_HOLDER_H

#include "score/mw/log/logging.h"

#include "score/mw/com/types.h"

#include <score/assert.hpp>
#include <score/callback.hpp>
#include <score/stop_token.hpp>
#include <score/utility.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace score::mw::service::backend::mw_com::internal
{

template <typename MwComProxy>
class ProxyHolder;

template <typename MwComProxy>
class ProxyHolderCreator
{
  public:
    std::shared_ptr<ProxyHolder<MwComProxy>> operator()(std::string instance_specifier)
    {
        return ProxyHolder<MwComProxy>::CreateFor(std::move(instance_specifier));
    }
};

///
/// @brief Utility class for encapsulating the logic of managing service discovery and found notifications of a proxy.
///
/// A ProxyHolder must be created as a shared_ptr via CreateFor(). This is because its lifetime must be shared between
/// the FindStrategy which owns the ProxyHolder and the find service handler provided to StartFindService. This is
/// important in case the find service handler outlives the FindStrategy (which owns the ProxyHolder) which
/// can happen if the ProxyFuture owning the strategy is destroyed during the call to the find service handler. This can
/// occur when using continuations on the ProxyFuture (see
/// score/mw/service/backend/mw_com/proxy_holder_robustness_test.cpp for an example).
template <typename MwComProxy>
class ProxyHolder final : public std::enable_shared_from_this<ProxyHolder<MwComProxy>>
{
    using HandleType = typename MwComProxy::HandleType;
    using MwComProxyHandlesContainer = score::mw::com::ServiceHandleContainer<HandleType>;

    /// @brief private type so that ProxyHolder's constructor can only be called by its CreateFor() factory methods
    class ConstructionTag
    {
      public:
        constexpr explicit ConstructionTag() noexcept = default;
    };

  public:
    using OnServiceFoundCallback = score::cpp::callback<void(ProxyHolder&), 128>;

    /// @brief factory method for creating a ProxyHolder for an instance specifier
    static std::shared_ptr<ProxyHolder> CreateFor(std::string instance_specifier)
    {
        return std::make_shared<ProxyHolder>(std::move(instance_specifier), ConstructionTag{});
    }

    // The ProxyHolder class is a template class, each translation unit will instantiate the template class
    // separately, which violates the ODR.
    // coverity[autosar_cpp14_m3_2_2_violation] see above justification
    // Suppress "AUTOSAR C++14 A12-1-2", since instance_specifier needs to be initialized using constructor parameter
    // and all other member variables are not value initialized coverity
    // coverity[autosar_cpp14_a12_1_2_violation : FALSE] see above justification
    ProxyHolder(std::string instance_specifier, const ConstructionTag) noexcept;

    ~ProxyHolder() noexcept;

    constexpr ProxyHolder(ProxyHolder&&) noexcept = delete;
    constexpr ProxyHolder(const ProxyHolder&) noexcept = delete;
    constexpr ProxyHolder& operator=(ProxyHolder&&) noexcept = delete;
    constexpr ProxyHolder& operator=(const ProxyHolder&) noexcept = delete;

    /// @brief Start the (asynchronous) service discovery functionality for `MwComProxy`
    ///        together with a callback that shall be invoked once the service got found.
    ///
    /// @details This method will reinitiate service discovery each time it gets called.
    ///
    /// @param on_found callback to be invoked once `MwComProxy`'s corresponding service got found
    ///
    void StartFindService(OnServiceFoundCallback on_found = [](auto&) noexcept {});

    /// @brief Stop the (asynchronous) service discovery functionality for `MwComProxy` using the internally stored
    /// FindServiceHandle.
    ///
    /// This function would be called by InstantiationStrategyBase::StopFind() or any function that does not have
    /// access to the FindServiceHandle and wants to stop a previously offered service.
    ///
    void StopFindService() noexcept;

    /// @brief Extract all proxy instances which got found so far.
    auto ExtractProxies() noexcept;

  private:
    std::shared_ptr<ProxyHolder<MwComProxy>> GetSharedProxyHolder()
    {
        return this->shared_from_this();
    }

    /// @brief Create an instance of `MwComProxy` by using the provided proxy handle.
    ///
    /// @param proxy_handle handle to the underlying ara proxy's handle required for preconstruction
    /// @param service_name name of the proxy's corresponding service (used for logging puposes)
    ///
    /// @return std::unique_ptr to the created `MwComProxy` instance in case of successful preconstruction
    ///
    static std::unique_ptr<MwComProxy> ConstructUniqueProxyInstanceFrom(
        const HandleType& proxy_handle,
        const score::mw::com::InstanceSpecifier& service_name);

    /// @brief Stop the (asynchronous) service discovery process for `MwComProxy`.
    ///
    /// @note requires a `CheckedLock` to having been acquired by the caller
    void StopFindServiceImpl(const std::lock_guard<std::recursive_mutex>&) noexcept;

    std::optional<score::mw::com::FindServiceHandle> find_service_handle_{};

    std::set<HandleType> found_instances_{};
    std::vector<std::unique_ptr<MwComProxy>> found_proxies_{};
    score::mw::com::InstanceSpecifier instance_specifier_;

    std::recursive_mutex mutex_;
};

template <typename MwComProxy>
ProxyHolder<MwComProxy>::ProxyHolder(std::string instance_specifier, const ConstructionTag) noexcept
    : std::enable_shared_from_this<ProxyHolder<MwComProxy>>(),
      instance_specifier_{score::mw::com::InstanceSpecifier::Create(std::move(instance_specifier)).value()}
{
}

template <typename MwComProxy>
ProxyHolder<MwComProxy>::~ProxyHolder() noexcept
{
    const std::lock_guard lock{mutex_};
    StopFindServiceImpl(lock);
}

template <typename MwComProxy>
void ProxyHolder<MwComProxy>::StartFindService(OnServiceFoundCallback on_found)
{
    const std::lock_guard lock{mutex_};

    // clear left-overs from previous calls to `StartFindService()`
    StopFindServiceImpl(lock);
    found_instances_.clear();
    found_proxies_.clear();

    // Since score::mw::com::FindServiceHandler currently takes an score::cpp::callback of size 32 bytes and the
    // OnServiceFoundCallback is 128 bytes, it won't fit inside the callback so we must pass the state as a pointer to a
    // struct.
    class CallbackState
    {
      public:
        CallbackState(OnServiceFoundCallback&& on_found_callback,
                      std::shared_ptr<ProxyHolder<MwComProxy>> mw_com_proxy_instance_holder)
            : on_found{std::move(on_found_callback)}, mw_com_proxy_holder{mw_com_proxy_instance_holder}
        {
        }

        OnServiceFoundCallback on_found;
        std::shared_ptr<ProxyHolder<MwComProxy>> mw_com_proxy_holder;
    };

    // According to broken_link_c/issue/20236346, the callback is guaranteed to never be called
    // after StopFindService has returned. Since we call StopFindService in the destructor of ProxyHolder, we
    // can be sure that the pointer to ProxyHolder provided to the callback will always be valid when the
    // callback is called.
    auto on_service_found_mw_com_callback =
        [callback_state_ptr = std::make_unique<CallbackState>(std::move(on_found), GetSharedProxyHolder())](
            const MwComProxyHandlesContainer& proxy_handles,
            const score::mw::com::FindServiceHandle find_service_handle) noexcept {
            auto& proxy_holder = callback_state_ptr->mw_com_proxy_holder;

            std::unique_lock lock_of_callback{proxy_holder->mutex_};

            // Store the FindServiceHandle within ProxyHolder in case on_found_ calls StopFindService which will need to
            // call MwComProxy::StopFindService with this find_service_handle.
            proxy_holder->find_service_handle_ = find_service_handle;

            for (const auto& proxy_handle : proxy_handles)
            {
                const auto [_, handle_inserted] = proxy_holder->found_instances_.insert(proxy_handle);
                if (handle_inserted)
                {
                    score::cpp::ignore = proxy_holder->found_proxies_.emplace_back(
                        ConstructUniqueProxyInstanceFrom(proxy_handle, proxy_holder->instance_specifier_));

                    // To prevent lock-order-inversion issues between the mutexes in ProxyHolder and in
                    // InstantiationStrategyBase, we unlock the mutex before calling the on_found_ callback (which is
                    // provided by InstantiationStrategyBase). When calling StartFindService on
                    // InstantiationStrategyBase, the strategy's mutex is first locked and then the ProxyHolder's mutex
                    // is locked. When the handler an instance is found, the ProxyHolder callback will be called which
                    // locks the mutex. In the case that the ProxyFuture is destroyed during a call to the find service
                    // handler (which can occur using continuations on the ProxyFuture), the InstantiationStrategyBase
                    // mutex would eventually be locked when calling StopFind. This locking sequence is the opposite
                    // order to that when calling StartFindService. By unlocking here, we avoid the first lock of the
                    // ProxyHolder mutex and therefore avoid the lock-order-inversion issue.
                    lock_of_callback.unlock();
                    callback_state_ptr->on_found(*proxy_holder);
                    lock_of_callback.lock();
                }
            }
        };

    // start the service discovery functionality for `MwComProxy` now
    const auto start_find_service_result =
        MwComProxy::StartFindService(std::move(on_service_found_mw_com_callback), instance_specifier_);
    if (!start_find_service_result.has_value())
    {
        mw::log::LogError() << "StartFindService failed for service '" << instance_specifier_.ToString()
                            << "' failed due to error:" << start_find_service_result.error();
        return;
    }

    // **Recursive StopFindService**: If StopFindService was called synchronously within
    // on_service_found_mw_com_callback
    //   during StartFindService, then the returned FindServiceHandle will be invalid, since the search was already
    //   stopped. According to broken_link_c/issue/21792394, any subsequent calls to
    //   StopFindService with that FindServiceHandle will be quietly ignored. Therefore, we can safely store the
    //   find_service_handle_ here and the redundant call to StopFindService in the destructor of ProxyHolder will be
    //   ignored.
    //
    // **Recursive StartFindService**: If ProxyHolder::StartFindService was called synchronously within
    //   on_service_found_mw_com_callback during ProxyHolder::StartFindService, then the second invocation of
    //   StartFindService will first call ProxyHolder::StopFindService and then call its handler synchronously. The
    //   handler will then update find_service_handle_ with the handle from the second StartFindService call.
    //   Therefore, when the original StartFindService call returns, it will return the FindServiceHandle from the first
    //   call to StartFindService. Since this handle is no longer valid (as the search was already stopped), we do not
    //   overwrite the find_service_handle_ here as it will already contain the handle from the second StartFindService
    //   call.
    //
    //   In all other cases, ProxyHolder::StopFindService is called at the start of ProxyHolder::StartFindService so
    //   find_service_handle_ would not have a value and would therefore be set here.
    if (!find_service_handle_.has_value())
    {
        score::cpp::ignore = find_service_handle_.emplace(start_find_service_result.value());
    }
}

template <typename MwComProxy>
void ProxyHolder<MwComProxy>::StopFindService() noexcept
{
    const std::lock_guard lock{mutex_};
    StopFindServiceImpl(lock);
}

/// @brief Extract all proxy instances which got found so far.
template <typename MwComProxy>
auto ProxyHolder<MwComProxy>::ExtractProxies() noexcept
{
    const std::lock_guard lock{mutex_};
    decltype(found_proxies_) result{};
    std::swap(found_proxies_, result);
    return result;
}

template <typename MwComProxy>
std::unique_ptr<MwComProxy> ProxyHolder<MwComProxy>::ConstructUniqueProxyInstanceFrom(
    const HandleType& proxy_handle,
    const score::mw::com::InstanceSpecifier& service_name)
{
    auto proxy_creation_result = MwComProxy::Create(proxy_handle);
    if (!proxy_creation_result.has_value())
    {
        mw::log::LogError() << "Construction of proxy instance for service '" << service_name.ToString()
                            << "' failed due to error:" << proxy_creation_result.error();
        return nullptr;
    }

    return std::make_unique<MwComProxy>(std::move(proxy_creation_result).value());
}

template <typename MwComProxy>
void ProxyHolder<MwComProxy>::StopFindServiceImpl(const std::lock_guard<std::recursive_mutex>&) noexcept
{
    if (!find_service_handle_.has_value())
    {
        return;
    }

    const auto stop_find_service_result = MwComProxy::StopFindService(find_service_handle_.value());
    if (!stop_find_service_result.has_value())
    {
        mw::log::LogError() << "StopFindService for service '" << instance_specifier_.ToString()
                            << "' failed due to error:" << stop_find_service_result.error();
        return;
    }
    find_service_handle_.reset();
}

}  // namespace score::mw::service::backend::mw_com::internal

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROXY_HOLDER_H
