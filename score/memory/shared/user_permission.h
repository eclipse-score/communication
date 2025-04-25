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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_USER_PERMISSION_H
#define BASELIBS_SCORE_MEMORY_SHARED_USER_PERMISSION_H

#include "score/os/acl.h"

#include <sys/types.h>

#include <unordered_map>
#include <variant>
#include <vector>

namespace score::memory::shared::permission
{

struct WorldReadable
{
};
struct WorldWritable
{
};
using UserPermissionsMap = std::unordered_map<score::os::Acl::Permission, std::vector<uid_t>>;
using UserPermissions = std::variant<UserPermissionsMap, WorldReadable, WorldWritable>;

}  // namespace score::memory::shared::permission

#endif  // BASELIBS_SCORE_MEMORY_SHARED_USER_PERMISSION_H
