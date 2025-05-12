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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_DESTRUCTOR_NOTIFIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_DESTRUCTOR_NOTIFIER_H

#include <future>

namespace score::mw::com::impl::lola::test
{

/// \brief Helper class which sets the value of a provided promise on destruction
class DestructorNotifier
{
  public:
    explicit DestructorNotifier(std::promise<void>* handler_destruction_barrier) noexcept
        : handler_destruction_barrier_{handler_destruction_barrier}
    {
    }
    ~DestructorNotifier() noexcept
    {
        if (handler_destruction_barrier_ != nullptr)
        {
            handler_destruction_barrier_->set_value();
        }
    }

    DestructorNotifier(DestructorNotifier&& other) noexcept
        : handler_destruction_barrier_{other.handler_destruction_barrier_}
    {
        other.handler_destruction_barrier_ = nullptr;
    }

    DestructorNotifier(const DestructorNotifier&) = delete;
    DestructorNotifier& operator=(const DestructorNotifier&) = delete;
    DestructorNotifier& operator=(DestructorNotifier&&) = delete;

  private:
    std::promise<void>* handler_destruction_barrier_;
};

}  // namespace score::mw::com::impl::lola::test

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_DESTRUCTOR_NOTIFIER_H
