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
#include "score/mw/com/impl/bindings/lola/transaction_log.h"

namespace score::mw::com::impl::lola
{

TransactionLog::TransactionLog(const std::size_t number_of_slots,
                               memory::shared::ManagedMemoryResource& resource) noexcept
    : reference_count_slots_(number_of_slots, resource), subscribe_transactions_{}, subscription_max_sample_count_{}
{
}

}  // namespace score::mw::com::impl::lola
