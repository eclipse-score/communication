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
#ifndef SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H
#define SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H

#include "score/mw/com/impl/proxy_event_base.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <cstddef>
#include <functional>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

class ProxyFieldBase
{
  public:
    ProxyFieldBase(ProxyBase& proxy_base, ProxyEventBase* proxy_event_base_dispatch, std::string_view field_name)
        : proxy_base_{proxy_base}, proxy_event_base_dispatch_{proxy_event_base_dispatch}, field_name_{field_name}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_base_dispatch != nullptr);
    }

    /// \brief A ProxyFieldBase shall not be copyable
    ProxyFieldBase(const ProxyFieldBase&) = delete;
    ProxyFieldBase& operator=(const ProxyFieldBase&) = delete;

    /// \brief A ProxyFieldBase shall be moveable
    ProxyFieldBase(ProxyFieldBase&&) noexcept = default;
    ProxyFieldBase& operator=(ProxyFieldBase&&) noexcept = default;

    virtual ~ProxyFieldBase() noexcept = default;

    void UpdateProxyReference(ProxyBase& proxy_base) noexcept
    {
        proxy_base_ = proxy_base;
    }

    /**
     * \api
     * \brief Subscribe to the field.
     * \param max_sample_count Specify the maximum number of concurrent samples that this event shall
     *                          be able to offer to the using application.
     * \return On failure, returns an error code.
     */
    ResultBlank Subscribe(const std::size_t max_sample_count) noexcept
    {
        return proxy_event_base_dispatch_->Subscribe(max_sample_count);
    }

    /**
     * \api
     * \brief Get the subscription state of this field.
     * \details This method can always be called regardless of the state of the field.
     * \return Subscription state of the field.
     */
    SubscriptionState GetSubscriptionState() noexcept
    {
        return proxy_event_base_dispatch_->GetSubscriptionState();
    }

    /**
     * \api
     * \brief End subscription to a field and release needed resources.
     * \details It is illegal to call this method while data is still held by the application in the form of SamplePtr.
     *          Doing so will result in undefined behavior. After a call to this method, the field behaves as if it had
     *          just been constructed.
     */
    void Unsubscribe() noexcept
    {
        proxy_event_base_dispatch_->Unsubscribe();
    }

    /**
     * \api
     * \brief Get the number of samples that can still be received by the user of this field.
     * \details If this returns 0, the user first has to drop at least one SamplePtr before it is possible to receive
     *          data via GetNewSamples again. If there is no subscription for this field, the returned value is
     *          unspecified.
     * \return Number of samples that can still be received.
     */
    std::size_t GetFreeSampleCount() noexcept
    {
        return proxy_event_base_dispatch_->GetFreeSampleCount();
    }

    /**
     * \api
     * \brief Returns the number of new samples a call to GetNewSamples() (given parameter max_num_samples
     *        doesn't restrict it) would currently provide.
     * \details This is a proprietary extension to the official ara::com API. It is useful in resource sensitive
     *          setups, where the user wants to work in polling mode only without registered async receive-handlers.
     *          For further details see //score/mw/com/design/extensions/README.md.
     * \return Either 0 if no new samples are available (and GetNewSamples() wouldn't return any) or N, where 1 <= N <=
     *         actual new samples. I.e. an implementation is allowed to report a lower number than actual new samples,
     *         which would be provided by a call to GetNewSamples().
     */
    Result<std::size_t> GetNumNewSamplesAvailable() noexcept
    {
        return proxy_event_base_dispatch_->GetNumNewSamplesAvailable();
    }

    /**
     * \api
     * \brief Sets the handler to be called, whenever a new field value has been received.
     * \details Generally a ReceiveHandler has no restrictions on what mw::com API it is allowed to call.
     *          It is especially allowed to call all public APIs of the Field instance on which it had been
     *          set/registered as long as it obeys the general requirement, that API calls on a Proxy/Proxy field are
     *          thread safe/can't be called concurrently.
     * \attention This function MUST NOT be called from the context of a ReceiveHandler registered for this field!
     *            It makes semantically not really sense to register a "new" ReceiveHandler from the context of an
     *            already running ReceiveHandler. We also see no use cases for it and won't support it therefore.
     * \param handler user provided handler to be called
     */
    ResultBlank SetReceiveHandler(EventReceiveHandler handler) noexcept
    {
        return proxy_event_base_dispatch_->SetReceiveHandler(std::move(handler));
    }

    /**
     * \api
     * \brief Removes any ReceiveHandler registered via SetReceiveHandler.
     */
    ResultBlank UnsetReceiveHandler() noexcept
    {
        return proxy_event_base_dispatch_->UnsetReceiveHandler();
    }

  protected:
    std::reference_wrapper<ProxyBase> proxy_base_;
    ProxyEventBase* proxy_event_base_dispatch_;
    std::string_view field_name_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H
