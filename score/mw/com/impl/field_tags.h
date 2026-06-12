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
#ifndef SCORE_MW_COM_IMPL_FIELD_TAGS_H
#define SCORE_MW_COM_IMPL_FIELD_TAGS_H

#include <type_traits>

namespace score::mw::com::impl
{

/// \brief Tag types used on ProxyField/SkeletonField level to accomplish SFINAE gating of various signatures,
/// which depend on the availability of Get/Set/Notifier.
struct WithGetter
{
};

struct WithSetter
{
};

struct WithNotifier
{
};

namespace detail
{

template <typename TargetType, typename... Tags>
struct contains_type : std::disjunction<std::is_same<TargetType, Tags>...>
{
};

// SFINAE helper to check if a specific tag is contained in the provided tags.
template <typename FieldType, typename ClassFieldType, typename TargetTag, typename... Tags>
struct is_tag_enabled : std::conjunction<contains_type<TargetTag, Tags...>, std::is_same<FieldType, ClassFieldType>>
{
};

}  // namespace detail

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_FIELD_TAGS_H
