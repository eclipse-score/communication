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
#ifndef SCORE_MW_COM_EXAMPLE_RUST_METHOD_LOLA_E2E_CALC_GEN_H
#define SCORE_MW_COM_EXAMPLE_RUST_METHOD_LOLA_E2E_CALC_GEN_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com
{

struct Operands
{
    std::int32_t a;
    std::int32_t b;
};

struct Total
{
    std::int32_t value;
};

/// Mixed sizes and alignments, to catch argument packing that disagrees with the C++ layout.
struct Wide
{
    std::uint8_t a;
    std::uint64_t b;
    std::uint16_t c;
    double d;
};

/// Methods of every shape (one argument, two arguments, none, void return, a slow handler), so a Rust
/// producer and a Rust consumer in different processes can exercise them over the LoLa runtime.
template <typename Trait>
class CalcInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Method<Total(Operands)> add{*this, "add"};
    typename Trait::template Method<Wide(Wide, Wide)> mix{*this, "mix"};
    typename Trait::template Method<Total()> ping{*this, "ping"};
    typename Trait::template Method<void(Total)> reset{*this, "reset"};
    typename Trait::template Method<Total()> last{*this, "last"};
    typename Trait::template Method<Total(Total)> slow{*this, "slow"};
};

using CalcProxy = AsProxy<CalcInterface>;
using CalcSkeleton = AsSkeleton<CalcInterface>;

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_EXAMPLE_RUST_METHOD_LOLA_E2E_CALC_GEN_H
