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

#ifndef SCORE_MW_SERVICE_SINGLE_INSTANCE_HOLDER_H
#define SCORE_MW_SERVICE_SINGLE_INSTANCE_HOLDER_H

#include <memory>
#include <utility>

namespace score::mw::service
{

/// @brief A single-template-parameter alias for the type holding a single instance
/// @tparam InstanceType the (base) type of the instance which shall be stored
template <typename InstanceType>
using SingleInstanceHolder = std::unique_ptr<InstanceType>;

/// @brief Utility type encompassing the logic for creating a `SingleInstanceHolder` from an implementation object
/// @tparam InstanceType the (base) type of the instance which shall be created
template <typename InstanceType>
class SingleInstanceHolderCreator
{
    SingleInstanceHolderCreator() = delete;

  public:
    template <typename InstanceImplType, typename... Args>
    static SingleInstanceHolder<InstanceType> Construct(Args&&... args)
    {
        return std::make_unique<InstanceImplType>(std::forward<Args>(args)...);
    }
};

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_SINGLE_INSTANCE_HOLDER_H
