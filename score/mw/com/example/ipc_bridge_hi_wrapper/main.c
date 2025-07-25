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

#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Starting ipc_bridge_cpp_sil with the following arguments:\n");
    printf("-n 10 -m recv -t 2 -s /etc/mw_com_config.json\n");

    char *args[] = {"-n", "10", "-m", "recv", "-t", "2", "-s", "/etc/mw_com_config.json", NULL};
    execve("/usr/bin/ipc_bridge_cpp_sil", args, NULL);

    // If execve fails, print an error and sleep forever
    perror("execve failed");
    while (1) {
        printf("execve failed, sleeping...\n");
        sleep(10);
    }
    return 0;
}

