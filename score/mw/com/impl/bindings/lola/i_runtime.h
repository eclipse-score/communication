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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_RUNTIME_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_RUNTIME_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/rollback_synchronization.h"
#include "score/mw/com/impl/configuration/shm_size_calc_mode.h"
#include "score/mw/com/impl/i_runtime_binding.h"

#include <cstdint>

namespace score::mw::com::impl::lola
{

/// \brief LoLa binding specific runtime interface
class IRuntime : public impl::IRuntimeBinding
{
  public:
    IRuntime() noexcept = default;

    virtual ~IRuntime() noexcept = default;

    /// \brief returns the message passing service instance needed by/used by LoLa skeletons/proxies.
    /// \return message passing service instance
    virtual lola::IMessagePassingService& GetLolaMessaging() noexcept = 0;

    /// \brief returns, whether LoLa binding runtime was created with ASIL-B support
    virtual bool HasAsilBSupport() const noexcept = 0;

    /// \brief returns configured mode, how shm-sizes shall be calculated.
    virtual ShmSizeCalculationMode GetShmSizeCalculationMode() const noexcept = 0;

    virtual RollbackSynchronization& GetRollbackSynchronization() noexcept = 0;

    /// \brief We need our PID in several locations/frequently. So the runtime shall provide/cache it.
    virtual pid_t GetPid() const noexcept = 0;
    /// \brief We need our Application ID in several locations/frequently. So the runtime shall provide/cache it.
    virtual std::uint32_t GetApplicationId() const noexcept = 0;

  protected:
    IRuntime(IRuntime&&) noexcept = default;
    IRuntime& operator=(IRuntime&&) noexcept = default;
    IRuntime(const IRuntime&) noexcept = default;
    IRuntime& operator=(const IRuntime&) noexcept = default;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_RUNTIME_H
