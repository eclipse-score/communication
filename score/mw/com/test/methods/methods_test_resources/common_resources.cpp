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
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"

#include "score/mw/com/runtime.h"

namespace score::mw::com::test
{

auto CreateInterprocessNotificationShmPath(size_t path_id) -> std::string
{
    std::string path{"test_methods_interprocess_shm_notification_path"};
    path.append(std::to_string(path_id));
    path.append("_notification");
    return path;
}

void InitializeRuntime(const std::string& path)
{
    auto rtc = path.empty() ? score::mw::com::runtime::RuntimeConfiguration()
                            : score::mw::com::runtime::RuntimeConfiguration(path);
    score::mw::com::runtime::InitializeRuntime(rtc);
    score::mw::log::LogInfo() << "LoLa Runtime initialized!";
}

}  // namespace score::mw::com::test
