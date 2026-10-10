// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#pragma once

#include <type_traits>

namespace test
{

/// \brief Enabled only for integral types (SFINAE in the return type).
/// \api
template <typename T>
std::enable_if_t<std::is_integral<T>::value, T> DoubleValue(T v);

/// \brief Enabled only for signed types (trailing-return SFINAE).
/// \api
template <typename T>
auto Negate(T v) -> std::enable_if_t<std::is_signed<T>::value, T>;

}  // namespace test
