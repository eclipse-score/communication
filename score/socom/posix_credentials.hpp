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

#ifndef SCORE_SOCOM_POSIX_CREDENTIALS_HPP
#define SCORE_SOCOM_POSIX_CREDENTIALS_HPP

#include <sys/types.h>
namespace score::socom {

/// \brief Posix_credentials.
struct Posix_credentials final {
    /// \brief user ID
    ::uid_t uid;
    /// \brief group ID
    ::gid_t gid;
};

}  // namespace score::socom

#endif  // SCORE_SOCOM_POSIX_CREDENTIALS_HPP
