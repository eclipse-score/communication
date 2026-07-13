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

#include <cstdint>
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

/// \brief Runtime form of the WithNotifier tag, for places the tag pack cannot reach (e.g. the binding
/// factories).
enum class FieldNotifier : std::uint8_t
{
    kEnabled = 0U,
    kDisabled
};

template <typename TargetType, typename... Tags>
struct contains_type : std::disjunction<std::is_same<TargetType, Tags>...>
{
};

/// \brief Gates a member function on whether a capability tag was declared for this field.
///
/// Two things have to be true for the overload to work:
///   - \c RequiredTag is somewhere(order does not matter) in \c EnabledTags.
///   - \c FieldType == \c ClassFieldType (see below)
///
/// The second condition (\c FieldType == \c ClassFieldType) is a SFINAE workaround: \c FieldType
/// must be a deduced local parameter of the member function template (defaulting to \c ClassFieldType)
/// so the check stays dependent and the compiler does not harderr at class instantiation time.
///
/// In practice you use it like this inside \c Foo<FieldType, Tags...>:
/// \code
///   template <typename F = FieldType,
///             std::enable_if_t<is_tag_enabled<F, FieldType, WithSetter, Tags...>::value, int> = 0>
///   void Set(F value);
/// \endcode
///
/// \tparam FieldType       Local deduced copy of the field type, must default to \c ClassFieldType.
/// \tparam ClassFieldType  The actual field type the enclosing class was instantiated with.
/// \tparam RequiredTag     The tag that must be present (e.g. \c WithSetter, \c WithGetter).
/// \tparam EnabledTags     All tags the field was declared with.
template <typename FieldType, typename ClassFieldType, typename RequiredTag, typename... EnabledTags>
struct is_tag_enabled
    : std::conjunction<contains_type<RequiredTag, EnabledTags...>, std::is_same<FieldType, ClassFieldType>>
{
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_FIELD_TAGS_H
