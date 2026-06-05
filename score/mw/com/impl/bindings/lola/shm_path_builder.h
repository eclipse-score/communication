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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SHM_PATH_BUILDER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SHM_PATH_BUILDER_H

#include "score/mw/com/impl/bindings/lola/i_shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/proxy_instance_identifier.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include <string>

namespace score::mw::com::impl::lola
{

/// Utility class to generate paths to the shm files.
///
/// See IShmPathBuilder for details
class ShmPathBuilder : public IShmPathBuilder
{
  public:
    /// \brief Constructs a ShmPathBuilder for the given service.
    ///
    /// \param service_id       Numeric identifier of the LoLa service.
    /// \param inter_vm_support When \c true, the prefix \c "/intervm-shared-shmem/" is prepended
    ///                         to all generated SHM object path names.
    ///                         \note This is an interim solution to express the intent to have a
    ///                         shared-memory object that supports sharing across VMs by encoding it
    ///                         into the path name. A future solution will be based on a redesigned
    ///                         \c lib/memory/shared API abstraction.
    explicit ShmPathBuilder(const std::uint16_t service_id, const bool inter_vm_support = false) noexcept
        : IShmPathBuilder{}, service_id_{service_id}, inter_vm_support_{inter_vm_support}
    {
    }

    std::string GetDataChannelShmName(const LolaServiceInstanceId::InstanceId instance_id) const noexcept override;

    std::string GetControlChannelShmName(const LolaServiceInstanceId::InstanceId instance_id,
                                         const QualityType channel_type) const noexcept override;

    std::string GetMethodChannelShmName(
        const LolaServiceInstanceId::InstanceId instance_id,
        const ProxyInstanceIdentifier& proxy_instance_identifier) const noexcept override;

  private:
    const std::uint16_t service_id_;
    const bool inter_vm_support_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SHM_PATH_BUILDER_H
