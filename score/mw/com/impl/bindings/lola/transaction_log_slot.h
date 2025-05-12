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
    TransactionLogSlot() noexcept : transaction_begin_{0U}, transaction_end_{0U} {}

    void SetTransactionBegin(bool new_value) noexcept
    {
        transaction_begin_ = static_cast<std::uint8_t>(new_value);
    }
    void SetTransactionEnd(bool new_value) noexcept
    {
        transaction_end_ = static_cast<std::uint8_t>(new_value);
    }

    bool GetTransactionBegin() const noexcept
    {
        return static_cast<bool>(transaction_begin_);
    }
    bool GetTransactionEnd() const noexcept
    {
        return static_cast<bool>(transaction_end_);
    }

  private:
    std::uint8_t transaction_begin_ : 1;
    std::uint8_t transaction_end_ : 2;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TRANSACTION_LOG_SLOT_H
