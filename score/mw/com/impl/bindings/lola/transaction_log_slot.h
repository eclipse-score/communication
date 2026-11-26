/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SLOT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SLOT_H

#include <cstdint>

namespace score::mw::com::impl::lola
{

class TransactionLogSlot
{
  public:
    TransactionLogSlot() noexcept : flags_{0U} {}

    void SetTransactionBegin(bool new_value) noexcept
    {
        SetFlag(kTransactionBeginMask, new_value);
    }
    void SetTransactionEnd(bool new_value) noexcept
    {
        SetFlag(kTransactionEndMask, new_value);
    }

    bool GetTransactionBegin() const noexcept
    {
        return (flags_ & kTransactionBeginMask) != 0U;
    }
    bool GetTransactionEnd() const noexcept
    {
        return (flags_ & kTransactionEndMask) != 0U;
    }

  private:
    static constexpr std::uint8_t kTransactionBeginMask{0x01U};
    static constexpr std::uint8_t kTransactionEndMask{0x02U};

    void SetFlag(const std::uint8_t mask, const bool new_value) noexcept
    {
        flags_ = static_cast<std::uint8_t>(new_value ? (flags_ | mask) : (flags_ & static_cast<std::uint8_t>(~mask)));
    }

    // Both flags are packed into a single byte, as this class is placed in shared memory.
    std::uint8_t flags_;
};

static_assert(sizeof(TransactionLogSlot) == 1U, "TransactionLogSlot must occupy exactly one byte in shared memory.");

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SLOT_H
