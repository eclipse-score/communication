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
#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/methods/method_signature_element_ptr.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Primary template of ProxyMethod. This is the "catch all" case for ProxyMethod class template instantiation.
/// \details This template only exists to provide a static_assert, that gives a meaningful error message, when
/// ProxyMethod is instantiated with an unsupported signature type.
template <typename Signature>
class ProxyMethod
{
    static_assert(
        sizeof(Signature) == 0,
        "ProxyMethod can only be instantiated with a function signature, e.g., ProxyMethod<void(int, double)>. "
        "You tried to use an unsupported signature type.");
};

/// \brief View on ProxyMethod to provide access to internal type-erased type information.
template <typename ReturnType, typename... ArgTypes>
class ProxyMethodView
{
  public:
    explicit ProxyMethodView(ProxyMethod<ReturnType(ArgTypes...)>& proxy_method) : proxy_method_{proxy_method} {}

    std::optional<memory::DataTypeSizeInfo> GetTypeErasedReturnType() const noexcept
    {
        return proxy_method_.type_erased_return_type_;
    }

    std::optional<memory::DataTypeSizeInfo> GetTypeErasedInArgs() const noexcept
    {
        return proxy_method_.type_erased_in_args_;
    }

  private:
    ProxyMethod<ReturnType(ArgTypes...)>& proxy_method_;
};

namespace detail
{

/// \brief Determines the next available queue slot in the case of a method call with in-args, where it needs to be
/// checked, whether MethodInArgPtr arguments are still active.
/// \return If there is an available queue slot, returns its index. Otherwise, returns ComErrc::kCallQueueFull.
template <typename... ArgTypes>
score::Result<std::size_t> DetermineNextAvailableQueueSlot(
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>>& in_arg_ptr_flags,
    containers::DynamicArray<bool>& return_type_ptr_flags)
{
    // Find an index, where both are_in_arg_ptrs_active_ and is_return_type_ptr_active_ are inactive
    for (std::size_t i = 0U; i < in_arg_ptr_flags.size(); ++i)
    {
        bool all_inactive = std::none_of(in_arg_ptr_flags[i].begin(), in_arg_ptr_flags[i].end(), [](bool active) {
            return active;
        });
        if (all_inactive && (!return_type_ptr_flags[i]))
        {
            // Found an available slot
            return i;
        }
    }
    return score::MakeUnexpected(ComErrc::kCallQueueFull);
}

/// \brief Determines the next available queue slot in the case of a method call without in-args, where only the
/// return type pointer needs to be checked.
/// \return If there is an available queue slot, returns its index. Otherwise, returns ComErrc::kCallQueueFull.
score::Result<std::size_t> DetermineNextAvailableQueueSlot(containers::DynamicArray<bool>& return_type_ptr_flags);

/// \brief Creates a tuple of MethodInArgPtr for the given argument types from the given tuple of raw pointers.
/// \tparam I Compile-time index sequence for the argument types.
/// \param ptrs Tuple of raw pointers to the argument values.
/// \param in_arg_ptr_flags Dynamic array of in-argument pointer active flags.
/// \param queue_index Queue index to be set in each MethodInArgPtr.
/// \return Tuple of MethodInArgPtr for each argument type.
template <typename... ArgTypes, std::size_t... I>
std::tuple<impl::MethodInArgPtr<ArgTypes>...> CreateMethodInArgPtrTuple(
    const std::tuple<ArgTypes*...>& ptrs,
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>>& in_arg_ptr_flags,
    std::size_t queue_index,
    std::index_sequence<I...>)
{
    return std::make_tuple(
        MethodInArgPtr<ArgTypes>(*(std::get<I>(ptrs)), in_arg_ptr_flags[queue_index][I], queue_index)...);
}

/// \brief Allocates in-argument storage for a ProxyMethod with in-arguments. Helper method used by all ProxyMethod
/// template specializations with in-arguments.
/// \details Asserts, that the binding successfully allocated the in-argument storage, if we determined an available
/// queue slot on the binding independent level.
/// \return either a tuple of MethodInArgPtr for each argument type or an error code ComErrc::kCallQueueFull
template <typename... ArgTypes>
score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> AllocateImpl(
    ProxyMethodBinding& binding,
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>>& in_arg_ptr_flags,
    containers::DynamicArray<bool>& return_type_ptr_flags)
{
    auto available_queue_slot = DetermineNextAvailableQueueSlot<ArgTypes...>(in_arg_ptr_flags, return_type_ptr_flags);
    if (!available_queue_slot.has_value())
    {
        return Unexpected(available_queue_slot.error());
    }
    const std::size_t queue_index = available_queue_slot.value();
    auto allocated_in_args_storage = binding.AllocateInArgs(queue_index);
    if (!allocated_in_args_storage.has_value())
    {
        return Unexpected(allocated_in_args_storage.error());
    }
    const auto deserialized_arg_pointers = impl::Deserialize<ArgTypes...>(allocated_in_args_storage.value());
    auto method_in_arg_ptr_tuple = CreateMethodInArgPtrTuple(
        deserialized_arg_pointers, in_arg_ptr_flags, queue_index, std::make_index_sequence<sizeof...(ArgTypes)>());

    return score::Result<std::tuple<score::mw::com::impl::MethodInArgPtr<ArgTypes>...>>(std::move(method_in_arg_ptr_tuple));
}

/// \brief Checks, that all MethodInArgPtr arguments have the same queue_position_ and returns this common value.
/// \details Will assert/terminate, if the queue_position_ values differ.
/// \tparam MethodInArgPtrs Variadic template parameter pack for MethodInArgPtr types.
/// \param args MethodInArgPtr arguments to check.
/// \return The common queue_position_ value.
///
/// \remark We are not checking, whether the user handed over MethodInArgPtr from different ProxyMethod
/// instances. This would require more type information at runtime, which we don't have. Therefore, we just check,
/// that all queue_position_ values are the same.
template <typename... MethodInArgPtrs>
std::size_t GetCommonQueuePosition(const MethodInArgPtrs&... args)
{
    std::optional<std::size_t> expected_queue_position{};
    (([&expected_queue_position](const auto& method_in_arg_ptr) {
         if (!expected_queue_position.has_value())
         {
             expected_queue_position = method_in_arg_ptr.GetQueuePosition();
         }
         else
         {
             if (method_in_arg_ptr.GetQueuePosition() != expected_queue_position.value())
             {
                 SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "All MethodInArgPtr arguments must have the same queue_position_");
             }
         }
     })(args),
     ...);
    return expected_queue_position.value();
}

}  // namespace detail
}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_H
