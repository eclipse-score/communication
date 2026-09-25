/*********************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_DETAILS_TYPE_HOLDER_H
#define SCORE_MW_SERVICE_DETAILS_TYPE_HOLDER_H

namespace score::mw::service::details
{

// We need a default constructible type to pass it down tuple iterator functions. Since the types we want to store in
// the tuples (only the type information, no value), might not be default constructible, we have to wrap them.
template <typename T>
struct TypeHolder
{
    using Types = T;
};

}  // namespace score::mw::service::details

#endif  // SCORE_MW_SERVICE_DETAILS_TYPE_HOLDER_H
