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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_PARTIAL_RESTART_PATH_BUILDER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_PARTIAL_RESTART_PATH_BUILDER_H

#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include <string>

namespace score::mw::com::impl::lola
{

/// \brief Utility class to generate paths related to Partial Restart
class IPartialRestartPathBuilder
{
  public:
    IPartialRestartPathBuilder() noexcept = default;

    virtual ~IPartialRestartPathBuilder() noexcept = default;

    /// Returns the path for the lock file used to indicate existence of service instance.
    virtual std::string GetServiceInstanceExistenceMarkerFilePath(
        const LolaServiceInstanceId::InstanceId instance_id) const noexcept = 0;

    /// Returns the path for the lock file used to indicate usage of service instance.
    virtual std::string GetServiceInstanceUsageMarkerFilePath(
        const LolaServiceInstanceId::InstanceId instance_id) const noexcept = 0;

    /// Returns the path for folder where partial restart specific files shall be stored.
    virtual std::string GetLolaPartialRestartDirectoryPath() const noexcept = 0;

  protected:
    IPartialRestartPathBuilder(const IPartialRestartPathBuilder&) noexcept = default;
    IPartialRestartPathBuilder& operator=(const IPartialRestartPathBuilder&) noexcept = default;
    IPartialRestartPathBuilder(IPartialRestartPathBuilder&& other) noexcept = default;
    IPartialRestartPathBuilder& operator=(IPartialRestartPathBuilder&& other) noexcept = default;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_PARTIAL_RESTART_PATH_BUILDER_H
