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
#ifndef PLATFORM_AAS_MW_COM_IMPL_METHOD_TYPE_H
#define PLATFORM_AAS_MW_COM_IMPL_METHOD_TYPE_H

#include <cstdint>
#include <string_view>

namespace score::mw::com::impl
{

namespace detail
{

/// \brief Tag types used on ProxyField/SkeletonField level to accomplish overload-resolution for various signatures,
/// which depend on the availability of Get/Set/Notifier.
struct EnableBothTag
{
};
struct EnableGetOnlyTag
{
};
struct EnableSetOnlyTag
{
};
struct EnableNeitherTag
{
};

}  // namespace detail

/// \brief Enum used to differentiate between regular service methods and field Get/Set methods.
enum class MethodType : std::uint8_t
{
    kUnknown = 0U,
    kMethod,
    kGet,
    kSet
};

std::string_view to_string(MethodType method_type) noexcept;

}  // namespace score::mw::com::impl

#endif  // PLATFORM_AAS_MW_COM_IMPL_METHOD_TYPE_H
