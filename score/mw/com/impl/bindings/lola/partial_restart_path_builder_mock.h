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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PARTIAL_RESTART_PATH_BUILDER_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PARTIAL_RESTART_PATH_BUILDER_MOCK_H

#include "score/mw/com/impl/bindings/lola/i_partial_restart_path_builder.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola
{

class PartialRestartPathBuilderMock : public IPartialRestartPathBuilder
{
  public:
    MOCK_METHOD(std::string,
                GetServiceInstanceExistenceMarkerFilePath,
                (LolaServiceInstanceId::InstanceId instance_id),
                (const, noexcept, override));
    MOCK_METHOD(std::string,
                GetServiceInstanceUsageMarkerFilePath,
                (LolaServiceInstanceId::InstanceId instance_id),
                (const, noexcept, override));
    MOCK_METHOD(std::string, GetLolaPartialRestartDirectoryPath, (), (const, noexcept, override));
};

class PartialRestartPathBuilderFacade : public IPartialRestartPathBuilder
{
  public:
    PartialRestartPathBuilderFacade(PartialRestartPathBuilderMock& partial_restart_path_builder_mock)
        : IPartialRestartPathBuilder{}, partial_restart_path_builder_mock_{partial_restart_path_builder_mock}
    {
    }

    std::string GetServiceInstanceExistenceMarkerFilePath(
        const LolaServiceInstanceId::InstanceId instance_id) const noexcept override
    {
        return partial_restart_path_builder_mock_.GetServiceInstanceExistenceMarkerFilePath(instance_id);
    }

    std::string GetServiceInstanceUsageMarkerFilePath(
        const LolaServiceInstanceId::InstanceId instance_id) const noexcept override
    {
        return partial_restart_path_builder_mock_.GetServiceInstanceUsageMarkerFilePath(instance_id);
    }

    std::string GetLolaPartialRestartDirectoryPath() const noexcept override
    {
        return partial_restart_path_builder_mock_.GetLolaPartialRestartDirectoryPath();
    }

  private:
    PartialRestartPathBuilderMock& partial_restart_path_builder_mock_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PARTIAL_RESTART_PATH_BUILDER_MOCK_H
