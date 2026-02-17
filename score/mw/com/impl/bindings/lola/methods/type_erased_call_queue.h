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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_TYPE_ERASED_CALL_QUEUE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_TYPE_ERASED_CALL_QUEUE_H

#include "score/memory/data_type_size_info.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief Class which manages the memory required for the InTypes and ReturnType for a specific service Method
///
/// This class allocates the memory region for the call queue on construction and deallocates it on destruction.
class TypeErasedCallQueue final
{
  public:
    struct TypeErasedElementInfo
    {
        std::optional<memory::DataTypeSizeInfo> in_arg_type_info;
        std::optional<memory::DataTypeSizeInfo> return_type_info;
        std::size_t queue_size;
    };

    TypeErasedCallQueue(const memory::shared::MemoryResourceProxy& resource_proxy,
                        const TypeErasedElementInfo& type_erased_element_info);

    ~TypeErasedCallQueue();

    TypeErasedCallQueue(const TypeErasedCallQueue&) = delete;
    TypeErasedCallQueue& operator=(const TypeErasedCallQueue&) = delete;
    TypeErasedCallQueue(TypeErasedCallQueue&&) noexcept = delete;
    TypeErasedCallQueue& operator=(TypeErasedCallQueue&&) noexcept = delete;

    std::optional<score::cpp::span<std::byte>> GetInArgValuesQueueStorage() const;

    std::optional<score::cpp::span<std::byte>> GetReturnValueQueueStorage() const;

    auto GetTypeErasedElementInfo() const -> const TypeErasedElementInfo&;

  private:
    struct OffsetPtrSpan
    {
        memory::shared::OffsetPtr<std::byte> data;
        std::size_t size;
    };

    using InArgQueueSpan = OffsetPtrSpan;
    using ReturnQueueSpan = OffsetPtrSpan;

    std::pair<InArgQueueSpan, ReturnQueueSpan> AllocateQueue() const;

    const memory::shared::MemoryResourceProxy& resource_proxy_;

    TypeErasedElementInfo type_erased_element_info_;

    InArgQueueSpan in_args_queue_start_address_;
    ReturnQueueSpan return_queue_start_address_;
};

// Helper functions to get the storage pointer to a position in the queue of InArgValues / ReturnValues
score::cpp::span<std::byte> GetInArgValuesElementStorage(
    const size_t position,
    score::cpp::span<std::byte> in_arg_values_storage,
    const TypeErasedCallQueue::TypeErasedElementInfo& in_args_type_erased_info);

score::cpp::span<std::byte> GetReturnValueElementStorage(
    const size_t position,
    score::cpp::span<std::byte> return_value_storage,
    const TypeErasedCallQueue::TypeErasedElementInfo& return_type_erased_info);

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_TYPE_ERASED_CALL_QUEUE_H
