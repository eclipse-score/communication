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

#ifndef OUR_NAME_SPACE_IMPL_TYPE_COLLECTION_OF_TYPES_H
#define OUR_NAME_SPACE_IMPL_TYPE_COLLECTION_OF_TYPES_H

#include "score/memory/shared/map.h"
#include "score/memory/shared/string.h"
#include "score/memory/shared/vector.h"
#include <our/name_space/impl_type_somestruct.h>
#include <array>
#include <cstdint>

namespace our::name_space
{

struct CollectionOfTypes
{
    using allocator_type = ::score::memory::shared::PolymorphicOffsetPtrAllocator<>;

    CollectionOfTypes() = default;

    explicit CollectionOfTypes(const allocator_type& allocator) : l{allocator} {}

    CollectionOfTypes(const std::uint8_t& a_param,
                      const std::uint16_t& b_param,
                      const std::uint32_t& c_param,
                      const std::uint64_t& d_param,
                      const std::int8_t& e_param,
                      const std::int16_t& f_param,
                      const std::int32_t& g_param,
                      const std::int64_t& h_param,
                      const bool& i_param,
                      const float& j_param,
                      const double& k_param,
                      const ::our::name_space::SomeStruct& l_param,
                      const allocator_type& allocator = allocator_type())
        : a{a_param},
          b{b_param},
          c{c_param},
          d{d_param},
          e{e_param},
          f{f_param},
          g{g_param},
          h{h_param},
          i{i_param},
          j{j_param},
          k{k_param},
          l{l_param, allocator}
    {
    }

    CollectionOfTypes(const CollectionOfTypes& other, const allocator_type& allocator)
        : a{other.a},
          b{other.b},
          c{other.c},
          d{other.d},
          e{other.e},
          f{other.f},
          g{other.g},
          h{other.h},
          i{other.i},
          j{other.j},
          k{other.k},
          l{other.l, allocator}
    {
    }

    CollectionOfTypes(CollectionOfTypes&& other, const allocator_type& allocator)
        : a{std::move(other.a)},
          b{std::move(other.b)},
          c{std::move(other.c)},
          d{std::move(other.d)},
          e{std::move(other.e)},
          f{std::move(other.f)},
          g{std::move(other.g)},
          h{std::move(other.h)},
          i{std::move(other.i)},
          j{std::move(other.j)},
          k{std::move(other.k)},
          l{std::move(other.l), allocator}
    {
    }

    std::uint8_t a;
    std::uint16_t b;
    std::uint32_t c;
    std::uint64_t d;
    std::int8_t e;
    std::int16_t f;
    std::int32_t g;
    std::int64_t h;
    bool i;
    float j;
    double k;
    ::our::name_space::SomeStruct l;

    using IsEnumerableTag = void;
    template <typename F>
    void enumerate(F& fun)
    {
        fun(a);
        fun(b);
        fun(c);
        fun(d);
        fun(e);
        fun(f);
        fun(g);
        fun(h);
        fun(i);
        fun(j);
        fun(k);
        fun(l);
    }

    template <typename F>
    constexpr static void EnumerateTypes(F& fun)
    {
        fun.template Visit<decltype(a)>();
        fun.template Visit<decltype(b)>();
        fun.template Visit<decltype(c)>();
        fun.template Visit<decltype(d)>();
        fun.template Visit<decltype(e)>();
        fun.template Visit<decltype(f)>();
        fun.template Visit<decltype(g)>();
        fun.template Visit<decltype(h)>();
        fun.template Visit<decltype(i)>();
        fun.template Visit<decltype(j)>();
        fun.template Visit<decltype(k)>();
        fun.template Visit<decltype(l)>();
    }
};

}  // namespace our::name_space

#endif  // OUR_NAME_SPACE_IMPL_TYPE_COLLECTION_OF_TYPES_H
