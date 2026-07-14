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
#ifndef SCORE_MW_COM_TUTORIAL_CALCULATOR_SERVICE_H
#define SCORE_MW_COM_TUTORIAL_CALCULATOR_SERVICE_H

#include "score/mw/com/types.h"

#include <score/assert.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace score::mw::com::tutorial
{

// A fixed-capacity array of integers used as the (potentially large) argument type of the "arithmetic_mean" method.
//
// Method arguments are transported through shared memory. The middleware serializes them via a plain memcpy, therefore
// every method argument type must be *trivially copyable* (no heap-owning members like std::vector, no user-defined
// copy constructor/destructor). IntegerArray fulfils this: it only aggregates a std::array and a std::size_t.
//
// The array has a fixed maximum capacity (max_elements). How many of the leading elements actually carry a meaningful
// value is tracked separately in number_of_elements_. This lets a caller transport "up to max_elements" integers while
// the receiver only processes the first number_of_elements_ of them.
class IntegerArray
{
  public:
    static constexpr std::size_t max_elements = 1024U;

    // A default constructor is required because the zero-copy call path (ProxyMethod::Allocate()) default-constructs
    // the argument in the shared-memory slot before handing a pointer to it back to the caller.
    IntegerArray() = default;

    explicit IntegerArray(std::size_t number_of_elements) : number_of_elements_{number_of_elements}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(number_of_elements <= max_elements,
                                                    "number_of_elements exceeds max_elements");
    }

    void SetNumberOfElements(std::size_t number_of_elements)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(number_of_elements <= max_elements,
                                                    "number_of_elements exceeds max_elements");
        number_of_elements_ = number_of_elements;
    }

    std::size_t GetNumberOfElements() const
    {
        return number_of_elements_;
    }

    std::array<std::uint32_t, max_elements> internal_array_{};

  private:
    std::size_t number_of_elements_{0U};
};

static_assert(std::is_trivially_copyable_v<IntegerArray>,
              "IntegerArray must be trivially copyable to be usable as a method argument.");

// The CalculatorService exposes two *methods* (request/response). In contrast to events/fields (where the provider
// pushes data to the consumers) a method lets the consumer actively invoke an operation on the provider and receive a
// return value.
//
// A method is declared as `Trait::template Method<ReturnType(ArgTypes...)>`. On the skeleton (provider) side the
// application registers a handler that implements the method; on the proxy (consumer) side the method member becomes
// callable via operator().
template <typename Trait>
class CalculatorInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    // Adds two 32-bit values and returns their 64-bit sum. Both arguments are small scalars, so on the consumer side we
    // will use the convenient "copy" call operator, which takes the arguments by const-reference.
    typename Trait::template Method<std::uint64_t(std::uint32_t, std::uint32_t)> add_{*this, "add"};

    // Computes the arithmetic mean of the first number_of_elements integers stored in the passed IntegerArray. The
    // argument is deliberately large (up to ~4 KiB), so on the consumer side we will use the "zero-copy" call operator:
    // the caller first Allocate()s the argument directly inside the shared-memory slot and fills it in place, avoiding
    // any extra copy of the large array.
    typename Trait::template Method<std::uint32_t(IntegerArray)> arithmetic_mean_{*this, "arithmetic_mean"};
};

using CalculatorProxy = score::mw::com::AsProxy<CalculatorInterface>;
using CalculatorSkeleton = score::mw::com::AsSkeleton<CalculatorInterface>;

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_CALCULATOR_SERVICE_H
