/*******************************************************************************
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

#ifndef SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_ADDON_INTERFACE_H
#define SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_ADDON_INTERFACE_H

#include "score/mw/com/types.h"
#include <cstdint>
#include <ostream>

namespace score::mw::com::test
{

struct ExampleData
{
    std::uint16_t id;
    std::uint32_t value;
    bool valid;
};

inline bool operator==(const ExampleData& lhs, const ExampleData& rhs) noexcept
{
    return (lhs.id == rhs.id) && (lhs.value == rhs.value) && (lhs.valid == rhs.valid);
}

inline bool operator!=(const ExampleData& lhs, const ExampleData& rhs) noexcept
{
    return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& out, const ExampleData& data)
{
    out << "ExampleData{id: " << data.id << ", value: " << data.value << ", valid: " << data.valid << "}";
    return out;
}

template <typename T>
class AddonInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Event<ExampleData> example_event{*this, "example_event"};
    typename T::template Event<bool> active_event{*this, "active_event"};
};

using AddonInterfaceProxy = score::mw::com::AsProxy<AddonInterface>;
using AddonInterfaceSkeleton = score::mw::com::AsSkeleton<AddonInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_LOADING_ADD_ON_CONFIGURATION_ADDON_INTERFACE_H
