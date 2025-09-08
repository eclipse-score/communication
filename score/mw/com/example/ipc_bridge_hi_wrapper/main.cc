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
#include <cstring>
#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "HI_App: Starting ipc_bridge_cpp_sil" << std::endl;

    const char *c_args[] = {
        "/usr/bin/ipc_bridge_cpp_sil",
        "-n", "10",
        "-m", "recv",
        "-t", "200",
        "-s", "/etc/mw_com_config.json",
        nullptr
    };

    execve("/usr/bin/ipc_bridge_cpp_sil", const_cast<char* const*>(c_args), nullptr);

    std::cerr << "execve failed, sleeping... Reason: " << strerror(errno)
              << std::endl;
    while (true) {
      sleep(10);
    }
    return 0;
}

