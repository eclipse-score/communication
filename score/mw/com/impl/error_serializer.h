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
#ifndef SCORE_MW_COM_IMPL_ERROR_SERIALIZER_H
#define SCORE_MW_COM_IMPL_ERROR_SERIALIZER_H

#include "score/result/result.h"

#include <type_traits>

namespace score::mw::com::impl
{

/// \brief Class containing static methods which allow serializing / deserializing error types.
///
/// The serialized format represents No error with integer value 0. The error code is represented by its corresponding
/// integer value in the error code enum.
///
/// We use a class containing static methods so that we can explicitly instantiate
/// the class only for "allowed" error codes. It also allows us to perform checks on the error code type (e.g. testing
/// that the type adheres to the requirements specified below) and also allows us to make the allowed error codes
/// implementation dependencies (since the implementation can go in the source file with explicit template
/// instantiations). When using this class to serialize a new error code, **YOU MUST ADD AN EXPLICIT TEMPLATE
/// INSTANTIATION FOR YOUR ERROR CODE IN THE SOURCE FILE**
///
/// These error codes must adhere to the following requirements:
///     - The underlying integer type of the enum must be score::result::ErrorCode
///     - the integer value 0 of the error code must be kInvalid.
///     - the last element of the enum must be kNumEnumElements. (Note. this cannot be enforced in code within this
///     class, so must be manually checked by the user).
template <typename ErrorCode>
class ErrorSerializer
{
  public:
    /// \brief Type used to represent the error code in a serialized format.
    ///
    /// The serialized format is score::result::ErrorCode which is generally the underlying integer type of an error code.
    /// This ensures that the error type can always fit inside the seralized type.
    using SerializedErrorType = score::result::ErrorCode;
    static_assert(std::is_same_v<std::underlying_type_t<ErrorCode>, SerializedErrorType>);

    /// \brief Serializes the ComErrc into an integer format when there is no error
    ///        (e.g. a score::Result::has_value() == true)
    static SerializedErrorType SerializeSuccess();

    /// \brief Serializes the ComErrc into an integer format when there is an error
    ///        (e.g. a score::Result::has_value() == false)
    static SerializedErrorType SerializeError(const ErrorCode error_code);

    static score::ResultBlank Deserialize(const SerializedErrorType serialized_error);
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_ERROR_SERIALIZER_H
