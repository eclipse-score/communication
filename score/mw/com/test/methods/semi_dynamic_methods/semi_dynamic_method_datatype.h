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
#ifndef SCORE_MW_COM_TEST_METHODS_SEMI_DYNAMIC_METHODS_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_SEMI_DYNAMIC_METHODS_TEST_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include "score/mw/com/test/methods/semi_dynamic_methods/runtime_sized_array.h"

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SemiDynamicMethodInterface : public T::Base
{
  public:
    using T::Base::Base;

    using ArrayValueType = std::uint32_t;
    using SemiDynamicArray =
        RuntimeSizedArray<ArrayValueType, memory::shared::PolymorphicOffsetPtrAllocator<ArrayValueType>>;

    typename T::template Method<SemiDynamicArray(SemiDynamicArray)> with_in_args_and_return{*this,
                                                                                            "with_in_args_and_return"};
};

using SemiDynamicMethodProxy = score::mw::com::AsProxy<SemiDynamicMethodInterface>;
using SemiDynamicMethodSkeleton = score::mw::com::AsSkeleton<SemiDynamicMethodInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_SEMI_DYNAMIC_METHODS_TEST_METHOD_DATATYPE_H
