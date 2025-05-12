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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PARTIAL_RESTART_PATH_BUILDER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PARTIAL_RESTART_PATH_BUILDER_H

#include "score/mw/com/impl/bindings/lola/i_partial_restart_path_builder.h"

namespace score::mw::com::impl::lola
{

/// \brief Utility class to generate paths related to Partial Restart
class PartialRestartPathBuilder : public IPartialRestartPathBuilder
{
  public:
    /// Create a path builder.
    ///
    /// \param service_id ServiceId of path to be created
    explicit PartialRestartPathBuilder(std::uint16_t service_id) noexcept
        : IPartialRestartPathBuilder{}, service_id_{service_id}
    {
    }

    /// Returns the path for the lock file used to indicate existence of service instance.
    std::string GetServiceInstanceExistenceMarkerFilePath(
        const LolaServiceInstanceId::InstanceId instance_id) const noexcept override;

    /// Returns the path for the lock file used to indicate usage of service instance.
    std::string GetServiceInstanceUsageMarkerFilePath(
        const LolaServiceInstanceId::InstanceId instance_id) const noexcept override;

    /// Returns the path for folder where partial restart specific files shall be stored.
    std::string GetLolaPartialRestartDirectoryPath() const noexcept override;

  private:
    const std::uint16_t service_id_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PARTIAL_RESTART_PATH_BUILDER_H
