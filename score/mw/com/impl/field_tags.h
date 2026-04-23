/********************************************************************************
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
 ********************************************************************************/
#ifndef PLATFORM_AAS_MW_COM_IMPL_FIELD_TAGS_H
#define PLATFORM_AAS_MW_COM_IMPL_FIELD_TAGS_H

#include <cstdint>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief Tag types used on ProxyField/SkeletonField level to accomplish overload-resolution for various signatures,
/// which depend on the availability of Get/Set/Notifier.
struct WithGetter
{
};

struct WithSetter
{
};

}  // namespace score::mw::com::impl

#endif  // PLATFORM_AAS_MW_COM_IMPL_FIELD_TAGS_H
