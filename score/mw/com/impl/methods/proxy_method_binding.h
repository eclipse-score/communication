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
#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_BINDING_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_BINDING_H

#include "score/memory/data_type_size_info.h"
#include "score/result/result.h"

#include <score/span.hpp>
#include <score/stop_token.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl
{

/// \brief Interface a proxy method binding has to implement.
/// \details The binding layer is type agnostic. Therefore, all type information is passed in a type-erased manner via
/// TypeErasedDataTypeInfo instances.
class ProxyMethodBinding
{
  public:
    /// \brief Constructor
    /// \param in_args_type_erased_info Type-erased information about the in-arguments of the method. std::nullopt, if
    /// there are no in-arguments.
    /// \param return_type_type_erased_info Type-erased information about the return type of the method. std::nullopt,
    /// if the return type is void.
    ProxyMethodBinding(std::optional<memory::DataTypeSizeInfo> in_args_type_erased_info,
                       std::optional<memory::DataTypeSizeInfo> return_type_type_erased_info)
        : in_args_type_erased_info_(in_args_type_erased_info),
          return_type_type_erased_info_(return_type_type_erased_info)
    {
    }
    virtual ~ProxyMethodBinding() = default;

    /// \brief Allocates storage for the in-arguments of a method call at the given queue position.
    /// \param queue_position The call-queue position for which to allocate the in-arguments storage.
    /// \return span of bytes representing the allocated storage or an error.
    /// \note If the method has no in-arguments (i.e. in_args_type_erased_info_ is a std::nullopt), this method shall
    /// not be called.
    virtual score::Result<score::cpp::span<std::byte>> AllocateInArgs(std::size_t queue_position) = 0;

    /// \brief Allocates storage for the return type of a method call at the given queue position.
    /// \param queue_position The call-queue position for which to allocate the return type storage.
    /// \return span of bytes representing the allocated storage or an error.
    /// \note If the method has no return type (i.e. void, thus return_type_type_erased_info_ is a std::nullopt), this
    /// method shall not be called.
    virtual score::Result<score::cpp::span<std::byte>> AllocateReturnType(std::size_t queue_position) = 0;

    /// \brief Performs the actual method call at the given call-queue position.
    /// \details The in-arguments and return type storage must have been allocated before calling this method. The
    /// in-arguments must have been filled with the correct data before calling this method.
    /// \param queue_position The call-queue position at which to perform the method call.
    /// \return ResultBlank indicating success or failure of the method call.
    virtual score::ResultBlank DoCall(std::size_t queue_position) = 0;

  protected:
    std::optional<memory::DataTypeSizeInfo> in_args_type_erased_info_;
    std::optional<memory::DataTypeSizeInfo> return_type_type_erased_info_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_BINDING_H
