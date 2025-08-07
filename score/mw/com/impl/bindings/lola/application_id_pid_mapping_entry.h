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

    // \brief our key-type is a combination of 4 byte status and 4 byte uid
    using key_type = std::uint64_t;
    // \brief we use key_type for our lock-free sync algo -> atomic access needs to be always lock-free therefore
    static_assert(std::atomic<key_type>::is_always_lock_free);
    // \brief we are encoding the uid into our key-type and have foreseen 4 byte for it!
    static_assert(sizeof(uid_t) <= 4);

    /// \brief Load key atomically and return its parts as a pair.
    /// \return parts, which make up the key
    std::pair<MappingEntryStatus, uid_t> GetStatusAndUidAtomic() noexcept;
    void SetStatusAndUidAtomic(MappingEntryStatus status, uid_t application_id) noexcept;
    static key_type CreateKey(MappingEntryStatus status, uid_t application_id) noexcept;
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain so member variable can be safely accessed directly.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::atomic<key_type> key_application_id_status_{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t pid_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_ENTRY_H