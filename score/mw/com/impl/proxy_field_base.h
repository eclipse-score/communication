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

#include "score/mw/com/impl/enable_reference_to_moveable_from_this.h"
#include "score/mw/com/impl/proxy_event_base.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <cstddef>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

class ProxyFieldBaseView;

class ProxyFieldBase : public EnableReferenceToMoveableFromThis<ProxyFieldBase>
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyFieldBaseView;

  public:
    /// \param proxy_event_base_dispatch May be nullptr when the field's tag pack does not include WithNotifier.
    ProxyFieldBase(std::string_view field_name, ProxyEventBase* proxy_event_base_dispatch)
        : EnableReferenceToMoveableFromThis<ProxyFieldBase>(),
          proxy_event_base_dispatch_{proxy_event_base_dispatch},
          field_name_{field_name}
    {
    }

    /// \brief A ProxyFieldBase shall not be copyable
    ProxyFieldBase(const ProxyFieldBase&) = delete;
    ProxyFieldBase& operator=(const ProxyFieldBase&) = delete;

    /// \brief A ProxyFieldBase shall be moveable
    ProxyFieldBase(ProxyFieldBase&&) noexcept = default;
    ProxyFieldBase& operator=(ProxyFieldBase&&) noexcept = default;

    virtual ~ProxyFieldBase() noexcept = default;

    /**
     * \name Tag-gated interface
     *
     * These methods are protected rather than public because the derived \c ProxyField class
     * template decides which of them are exposed to the end user, based on the capability tags
     * (\c WithGetter, \c WithSetter, \c WithNotifier) the field was declared with.  Keeping them
     * protected here prevents direct access while still letting the derived class forward them
     * selectively — only when the matching tag is present.
     */
    /// \{
  protected:
    Result<void> Subscribe(const std::size_t max_sample_count) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->Subscribe(max_sample_count);
    }

    SubscriptionState GetSubscriptionState() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->GetSubscriptionState();
    }

    void Unsubscribe() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        proxy_event_base_dispatch_->Unsubscribe();
    }

    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->SetSubscriptionStateChangeHandler(std::move(handler));
    }

    Result<void> UnsetSubscriptionStateChangeHandler() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->UnsetSubscriptionStateChangeHandler();
    }

    std::size_t GetFreeSampleCount() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->GetFreeSampleCount();
    }

    Result<std::size_t> GetNumNewSamplesAvailable() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->GetNumNewSamplesAvailable();
    }

    Result<void> SetReceiveHandler(EventReceiveHandler handler) noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->SetReceiveHandler(std::move(handler));
    }

    Result<void> UnsetReceiveHandler() noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(proxy_event_base_dispatch_ != nullptr);
        return proxy_event_base_dispatch_->UnsetReceiveHandler();
    }
    /// \}

    ProxyEventBase* proxy_event_base_dispatch_;
    std::string_view field_name_;
};

/// \brief Controlled access to ProxyFieldBase internals for use within the impl namespace.
///
/// ProxyBase needs to call certain ProxyFieldBase methods (e.g. \c Unsubscribe) during its own
/// lifecycle, but those methods must not be reachable by end users of the public API because the ProxyField which
/// inherits from this class decides which functions are exposed to the user based on its template params. Broadly,
/// making ProxyBase a \c friend directly would give it unrestricted access to everything private.  This view class is
/// the middle ground. It is the only \c friend of ProxyFieldBase and selectively re-exposes just the methods that
/// ProxyBase actually needs.  End users never see it because it lives in the \c impl namespace.
class ProxyFieldBaseView
{
  public:
    explicit ProxyFieldBaseView(ProxyFieldBase& base) noexcept : base_{base} {}

    void Unsubscribe() noexcept
    {
        // if the WithNotifier tag is not set, proxy_event_base_dispatch_ will be nullptr.
        if (base_.proxy_event_base_dispatch_ != nullptr)
        {
            base_.Unsubscribe();
        }
    }

  private:
    ProxyFieldBase& base_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_FIELD_BASE_H
