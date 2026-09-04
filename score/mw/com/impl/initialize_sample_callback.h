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
#ifndef SCORE_MW_COM_IMPL_INITIALIZE_SAMPLE_CALLBACK_H
#define SCORE_MW_COM_IMPL_INITIALIZE_SAMPLE_CALLBACK_H

#include <score/callback.hpp>

namespace score::mw::com::impl
{

/// \brief Callback type for initializing a sample. The callback takes a pointer to the type-erased sample to be
/// initialized as an argument.
/// \details The callback is handed over from our strongly typed (binding independent) layer, which has the strong
/// type definition to the type-erased (binding) layers, in cases, where type-erased storage needs to be correctly
/// initialized.
using InitializeSampleCallback = score::cpp::callback<void(void*)>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_INITIALIZE_SAMPLE_CALLBACK_H
