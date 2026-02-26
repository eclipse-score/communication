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

#ifndef OUR_NAME_SPACE_IMPL_TYPE_SOME_STRUCT_H
#define OUR_NAME_SPACE_IMPL_TYPE_SOME_STRUCT_H

#include "score/memory/shared/map.h"
#include "score/memory/shared/string.h"
#include "score/memory/shared/vector.h"
#include <our/name_space/impl_type_multidimarray.h>
#include <our/name_space/impl_type_multidimvector.h>
#include <our/name_space/impl_type_myenum.h>
#include <our/name_space/impl_type_mytype.h>
#include <our/name_space/impl_type_somearray.h>
#include <our/name_space/impl_type_somevector.h>
#include <array>
#include <cstdint>

namespace our::name_space
{

struct SomeStruct
{
    using allocator_type = ::score::memory::shared::PolymorphicOffsetPtrAllocator<>;

    SomeStruct() = default;

    explicit SomeStruct(const allocator_type& allocator) : access_vector{allocator}, multi_dim_vector{allocator} {}

    SomeStruct(const std::uint8_t& foo_param,
               const std::uint16_t& bar_param,
               const ::our::name_space::SomeArray& access_array_param,
               const ::our::name_space::MultiDimArray& multi_dim_array_param,
#ifndef __QNX__
               ::score::memory::shared::String const& access_string_param,
#endif
               ::our::name_space::SomeVector const& access_vector_param,
               ::our::name_space::MultiDimVector const& multi_dim_vector_param,
               ::our::name_space::MyType const& my_type_param,
               ::our::name_space::MyEnum const& my_enum_param,
               allocator_type const& allocator = allocator_type())
        : foo{foo_param},
          bar{bar_param},
          access_array{access_array_param},
          multi_dim_array{multi_dim_array_param},
#ifndef __QNX__
          access_string{access_string_param},
#endif
          access_vector{access_vector_param, allocator},
          multi_dim_vector{multi_dim_vector_param, allocator},
          my_type{my_type_param},
          my_enum{my_enum_param}
    {
    }

    SomeStruct(const SomeStruct& other, const allocator_type& allocator)
        : foo{other.foo},
          bar{other.bar},
          access_array{other.access_array},
          multi_dim_array{other.multi_dim_array},
#ifndef __QNX__
          access_string{other.access_string},
#endif
          access_vector{other.access_vector, allocator},
          multi_dim_vector{other.multi_dim_vector, allocator},
          my_type{other.my_type},
          my_enum{other.my_enum}
    {
    }

    SomeStruct(SomeStruct&& other, const allocator_type& allocator)
        : foo{std::move(other.foo)},
          bar{std::move(other.bar)},
          access_array{std::move(other.access_array)},
          multi_dim_array{std::move(other.multi_dim_array)},
#ifndef __QNX__
          access_string{std::move(other.access_string)},
#endif
          access_vector{std::move(other.access_vector), allocator},
          multi_dim_vector{std::move(other.multi_dim_vector), allocator},
          my_type{std::move(other.my_type)},
          my_enum{std::move(other.my_enum)}
    {
    }

    std::uint8_t foo;
    std::uint16_t bar;
    ::our::name_space::SomeArray access_array;
    ::our::name_space::MultiDimArray multi_dim_array;
#ifndef __QNX__
    ::score::memory::shared::String access_string;
#endif
    ::our::name_space::SomeVector access_vector;
    ::our::name_space::MultiDimVector multi_dim_vector;
    ::our::name_space::MyType my_type;
    ::our::name_space::MyEnum my_enum;

    using IsEnumerableTag = void;
    template <typename F>
    void enumerate(F& fun)
    {
        fun(foo);
        fun(bar);
        fun(access_array);
        fun(multi_dim_array);
#ifndef __QNX__
        fun(access_string);
#endif
        fun(access_vector);
        fun(multi_dim_vector);
        fun(my_type);
        fun(my_enum);
    }

    template <typename F>
    constexpr static void EnumerateTypes(F& fun)
    {
        fun.template Visit<decltype(foo)>();
        fun.template Visit<decltype(bar)>();
        fun.template Visit<decltype(access_array)>();
        fun.template Visit<decltype(multi_dim_array)>();
#ifndef __QNX__
        fun.template Visit<decltype(access_string)>();
#endif
        fun.template Visit<decltype(access_vector)>();
        fun.template Visit<decltype(multi_dim_vector)>();
        fun.template Visit<decltype(my_type)>();
        fun.template Visit<decltype(my_enum)>();
    }
};

}  // namespace our::name_space

#endif  // OUR_NAME_SPACE_IMPL_TYPE_SOME_STRUCT_H
