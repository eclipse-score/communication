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
    explicit ShmPathBuilder(const std::uint16_t service_id) noexcept : IShmPathBuilder{}, service_id_{service_id} {}

    std::string GetDataChannelShmName(const LolaServiceInstanceId::InstanceId instance_id) const noexcept override;

    std::string GetControlChannelShmName(const LolaServiceInstanceId::InstanceId instance_id,
                                         const QualityType channel_type) const noexcept override;

    std::string GetMethodChannelShmName(
        const LolaServiceInstanceId::InstanceId instance_id,
        const ProxyInstanceIdentifier& proxy_instance_identifier) const noexcept override;

  private:
    const std::uint16_t service_id_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SHM_PATH_BUILDER_H
