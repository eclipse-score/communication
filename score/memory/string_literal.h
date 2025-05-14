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
#ifndef SCORE_LIB_MEMORY_STRING_LITERAL_H
#define SCORE_LIB_MEMORY_STRING_LITERAL_H

namespace score
{

/// \brief We cannot use score::cpp::string view everywhere, since it is not constexpr yet.
/// until then we need to still use string literals. In order to avoid the usage of `char` in the codebase
///  - which will lead to Klockwork warnings -, we have a custom definition here.
using StringLiteral = const char*;

}  // namespace score

#endif  // SCORE_LIB_MEMORY_STRING_LITERAL_H
