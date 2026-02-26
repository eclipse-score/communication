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
#include <thread>
#include <fstream>

int main()
{
    std::ifstream file("/tmp/Hello.txt");
    while (not file.is_open())
    {
        file = std::ifstream("/tmp/Hello.txt");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}
