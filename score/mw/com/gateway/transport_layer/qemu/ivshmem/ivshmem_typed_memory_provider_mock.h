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
#ifndef SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_IVSHMEM_TYPED_MEMORY_PROVIDER_MOCK_H
#define SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_IVSHMEM_TYPED_MEMORY_PROVIDER_MOCK_H

#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"

#include <gmock/gmock.h>

namespace score::mw::com::gateway::qemu::ivshmem
{

class IvshmemTypedMemoryProviderMock : public IvshmemTypedMemoryProvider
{
  public:
    IvshmemTypedMemoryProviderMock() noexcept : IvshmemTypedMemoryProvider{0U, 0U} {}

    MOCK_METHOD((score::cpp::expected_blank<score::os::Error>),
                AllocateNamedTypedMemory,
                (std::size_t, std::string, const score::memory::shared::permission::UserPermissions&),
                (const, noexcept, override));

    MOCK_METHOD(
        (score::cpp::expected_blank<score::os::Error>),
        AllocateNamedTypedMemoryAtOffset,
        (std::size_t, const std::string&, std::uint64_t, const score::memory::shared::permission::UserPermissions&),
        (const, noexcept, override));

    MOCK_METHOD((std::optional<std::uint64_t>),
                LookupOffsetInDirectory,
                (const std::string&),
                (const, noexcept, override));

    MOCK_METHOD((score::cpp::expected<int, score::os::Error>),
                AllocateAndOpenAnonymousTypedMemory,
                (std::uint64_t),
                (const, noexcept, override));

    MOCK_METHOD((score::cpp::expected_blank<score::os::Error>),
                Unlink,
                (std::string_view),
                (const, noexcept, override));

    MOCK_METHOD((score::cpp::expected<uid_t, score::os::Error>),
                GetCreatorUid,
                (std::string_view),
                (const, noexcept, override));
};

}  // namespace score::mw::com::gateway::qemu::ivshmem

#endif  // SCORE_MW_COM_GATEWAY_TRANSPORT_LAYER_QEMU_IVSHMEM_TYPED_MEMORY_PROVIDER_MOCK_H
