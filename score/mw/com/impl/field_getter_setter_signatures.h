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
#ifndef SCORE_MW_COM_IMPL_FIELD_GETTER_SETTER_SIGNATURES_H
#define SCORE_MW_COM_IMPL_FIELD_GETTER_SETTER_SIGNATURES_H

#include "score/result/result.h"

namespace score::mw::com::impl
{

/// \brief Provides the method signatures for field getter and setter methods.
///
/// Since the setter / getter functions can fail within the middleware code (e.g. when trying to update the field
/// value fails), we need to return a result from the setter and getter.
template <typename FieldType>
using SetMethodSignature = score::Result<FieldType>(FieldType);

template <typename FieldType>
using GetMethodSignature = score::Result<FieldType>();

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_FIELD_GETTER_SETTER_SIGNATURES_H
