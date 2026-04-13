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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_SHM_PATH_BUILDER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_SHM_PATH_BUILDER_H

#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"

#include "score/mw/com/impl/bindings/lola/proxy_instance_identifier.h"

#include <sys/types.h>
#include <cstdint>
#include <string>

namespace score::mw::com::impl::lola
{

/// Utility class to generate paths to the shm files.
///
/// There are up to three files per instance:
/// - The QM control file
/// - The ASIL B control file
/// - The data storage file
///
/// This class should be used to generate the paths to the files so that they can be mapped
/// into the processes address space for further usage.
///
/// The instance is identified by its LolaServiceInstanceDeployment. This object must outlive the lifetime
/// of ShmPathBuilder since the reference is stored inside the class.
class IShmPathBuilder
{
  public:
    IShmPathBuilder() noexcept = default;

    virtual ~IShmPathBuilder() noexcept = default;

    /// Returns the path suitable for shm_open to the data shared memory.
    ///
    /// \return The shm file name
    virtual std::string GetDataChannelShmName(const LolaServiceInstanceId::InstanceId instance_id) const noexcept = 0;

    /// Returns the path suitable for shm_open to the control shared memory.
    ///
    /// \param asil_b Whether to return the ASIL QM or ASIL B path.
    /// \return The shm file name
    virtual std::string GetControlChannelShmName(const LolaServiceInstanceId::InstanceId instance_id,
                                                 const QualityType channel_type) const noexcept = 0;

    /// Returns the path suitable for shm_open to the method shared memory.
    ///
    /// \param instance_id InstanceId of path to be created
    /// \param application_id which uniquely identifies a process (which defaults to user ID but can be set in
    ///        configuration)
    /// \param unique_identifier a unique identifier that will be appended to the shm name
    /// \return The shm file name
    virtual std::string GetMethodChannelShmName(
        const LolaServiceInstanceId::InstanceId instance_id,
        const ProxyInstanceIdentifier& proxy_instance_identifier) const noexcept = 0;

  protected:
    IShmPathBuilder(IShmPathBuilder&&) noexcept = default;
    IShmPathBuilder& operator=(IShmPathBuilder&&) noexcept = default;
    IShmPathBuilder(const IShmPathBuilder&) noexcept = default;
    IShmPathBuilder& operator=(const IShmPathBuilder&) noexcept = default;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_I_SHM_PATH_BUILDER_H
