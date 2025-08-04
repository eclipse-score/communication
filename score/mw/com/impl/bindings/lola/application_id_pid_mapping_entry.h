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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_ENTRY_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_ENTRY_H

#include <sys/types.h>
#include <atomic>
#include <cstdint>
#include <utility>

namespace score::mw::com::impl::lola
{

class ApplicationIdPidMappingEntry
{
  public:
    enum class MappingEntryStatus : std::uint16_t
    {
        kUnused = 0U,
        kUsed,
        kUpdating,
        kInvalid,  // this is a value, which we shall NOT see in an entry!
    };

    using key_type = std::uint64_t;
    static_assert(std::atomic<key_type>::is_always_lock_free);
    static_assert(sizeof(uid_t) <= 4);

    std::pair<MappingEntryStatus, uid_t> GetStatusAndUidAtomic() noexcept;
    void SetStatusAndUidAtomic(MappingEntryStatus status, uid_t application_id) noexcept;
    static key_type CreateKey(MappingEntryStatus status, uid_t application_id) noexcept;
    std::atomic<key_type> key_application_id_status_{};
    pid_t pid_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_ENTRY_H