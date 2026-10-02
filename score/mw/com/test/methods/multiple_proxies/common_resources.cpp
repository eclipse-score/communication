/********************************************************************************
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
 ********************************************************************************/
#include "score/mw/com/test/methods/multiple_proxies/common_resources.h"

#include <string>

namespace score::mw::com::test
{

std::string CreateInterprocessNotificationShmPath(size_t path_id)
{
    std::string path{"test_methods_interprocess_shm_notification_path"};
    path.append(std::to_string(path_id));
    path.append("_notification");
    return path;
}

}  // namespace score::mw::com::test
