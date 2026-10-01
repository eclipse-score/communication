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

#ifndef SCORE_MW_SERVICE_MANAGED_SERVICE_H
#define SCORE_MW_SERVICE_MANAGED_SERVICE_H

#include "score/result/result.h"

namespace score::mw::service
{

/// @brief This class serves as common base-class for any service managed via a `ProvidedServicesContainer`
class ManagedService
{
  public:
    constexpr ManagedService() noexcept = default;
    virtual ~ManagedService() noexcept = default;

    /// @brief Offers the underlying service in a way, that it can be found via service discovery on consumer side
    /// @return Result<void> indicating success or failure of the operation
    virtual Result<void> Start() = 0;

    /// @brief Stops offering the underlying service, thus it can no longer be found via service discovery
    virtual void Stop() noexcept = 0;

  protected:
    constexpr ManagedService(ManagedService&&) noexcept = default;
    constexpr ManagedService(const ManagedService&) noexcept = delete;
    ManagedService& operator=(ManagedService&&) & noexcept = default;
    ManagedService& operator=(const ManagedService&) & noexcept = delete;
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_MANAGED_SERVICE_H
