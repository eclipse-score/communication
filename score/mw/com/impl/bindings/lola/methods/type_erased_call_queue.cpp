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
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"

#include "score/memory/data_type_size_info.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert.hpp>

#include <cstddef>
#include <optional>
#include <tuple>

namespace score::mw::com::impl::lola
{
namespace
{

score::cpp::span<std::byte> GetElement(const std::size_t position,
                                const memory::DataTypeSizeInfo& type_info,
                                score::cpp::span<std::byte> queue_storage,
                                const std::size_t queue_size)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(position < queue_size);

    const auto element_offset = type_info.Size() * position;
    auto* const element_address = memory::shared::AddOffsetToPointer(queue_storage.data(), element_offset);

    return score::cpp::span{element_address, type_info.Size()};
}

}  // namespace

score::cpp::span<std::byte> GetInArgValuesElementStorage(
    const size_t position,
    score::cpp::span<std::byte> in_arg_values_storage,
    const TypeErasedCallQueue::TypeErasedElementInfo& in_args_type_erased_info)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(in_args_type_erased_info.in_arg_type_info.has_value());
    return GetElement(position,
                      in_args_type_erased_info.in_arg_type_info.value(),
                      in_arg_values_storage,
                      in_args_type_erased_info.queue_size);
}

score::cpp::span<std::byte> GetReturnValueElementStorage(
    const size_t position,
    score::cpp::span<std::byte> return_value_storage,
    const TypeErasedCallQueue::TypeErasedElementInfo& return_type_erased_info)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(return_type_erased_info.return_type_info.has_value());
    return GetElement(position,
                      return_type_erased_info.return_type_info.value(),
                      return_value_storage,
                      return_type_erased_info.queue_size);
}

TypeErasedCallQueue::TypeErasedCallQueue(const memory::shared::MemoryResourceProxy& resource_proxy,
                                         const TypeErasedElementInfo& type_erased_element_info)
    : resource_proxy_{resource_proxy},
      type_erased_element_info_{type_erased_element_info},
      in_args_queue_start_address_{nullptr},
      return_queue_start_address_{nullptr}
{
    // If we have neither InArgs nor a Return value, then we don't need to allocate any memory at all.
    if (!(type_erased_element_info_.in_arg_type_info.has_value() ||
          type_erased_element_info_.return_type_info.has_value()))
    {
        return;
    }
    std::tie(in_args_queue_start_address_, return_queue_start_address_) = AllocateQueue();
}

TypeErasedCallQueue::~TypeErasedCallQueue()
{
    if (in_args_queue_start_address_ != nullptr)
    {
        const auto& in_arg_type_info = type_erased_element_info_.in_arg_type_info.value();
        const auto allocated_size = in_arg_type_info.Size() * type_erased_element_info_.queue_size;
        resource_proxy_.deallocate(in_args_queue_start_address_.get(), allocated_size);
    }
    if (return_queue_start_address_ != nullptr)
    {
        const auto& return_type_info = type_erased_element_info_.return_type_info.value();
        const auto allocated_size = return_type_info.Size() * type_erased_element_info_.queue_size;
        resource_proxy_.deallocate(return_queue_start_address_.get(), allocated_size);
    }
}

std::optional<score::cpp::span<std::byte>> TypeErasedCallQueue::GetInArgValuesQueueStorage() const
{
    if (!(type_erased_element_info_.in_arg_type_info.has_value()))
    {
        return {};
    }
    return {{in_args_queue_start_address_.get(), type_erased_element_info_.in_arg_type_info->Size()}};
}

std::optional<score::cpp::span<std::byte>> TypeErasedCallQueue::GetReturnValueQueueStorage() const
{
    if (!(type_erased_element_info_.return_type_info.has_value()))
    {
        return {};
    }
    return {{return_queue_start_address_.get(), type_erased_element_info_.return_type_info->Size()}};
}

auto TypeErasedCallQueue::GetTypeErasedElementInfo() const -> const TypeErasedElementInfo&
{
    return type_erased_element_info_;
}

std::pair<memory::shared::OffsetPtr<std::byte>, memory::shared::OffsetPtr<std::byte>>
TypeErasedCallQueue::AllocateQueue() const
{
    std::byte* in_args_queue_start_address_bytes{nullptr};
    if (type_erased_element_info_.in_arg_type_info.has_value())
    {
        const auto& in_arg_type_info = type_erased_element_info_.in_arg_type_info.value();
        const auto required_size = in_arg_type_info.Size() * type_erased_element_info_.queue_size;
        void* const in_args_queue_start_address = resource_proxy_.allocate(required_size, in_arg_type_info.Alignment());
        in_args_queue_start_address_bytes = static_cast<std::byte*>(in_args_queue_start_address);
    }

    std::byte* return_queue_start_address_bytes{nullptr};
    if (type_erased_element_info_.return_type_info.has_value())
    {
        const auto& return_type_info = type_erased_element_info_.return_type_info.value();
        const auto required_size = return_type_info.Size() * type_erased_element_info_.queue_size;
        void* const return_queue_start_address = resource_proxy_.allocate(required_size, return_type_info.Alignment());
        return_queue_start_address_bytes = static_cast<std::byte*>(return_queue_start_address);
    }

    return {in_args_queue_start_address_bytes, return_queue_start_address_bytes};
}

}  // namespace score::mw::com::impl::lola
