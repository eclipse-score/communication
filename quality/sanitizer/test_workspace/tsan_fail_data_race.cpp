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
#include <cstdint>
#include <thread>

// This is a very basic example of a data-race condition, which should be found by a thread-sanitizer
int main()
{
    std::uint32_t some_number{0};

    std::thread first_thread{[&some_number]() {
        for (int i = 0; i < 100; i++)
        {
            some_number++;
        }
    }};

    std::thread second_thread{[&some_number]() {
        for (int i = 0; i < 100; i++)
        {
            some_number++;
        }
    }};

    first_thread.join();
    second_thread.join();
    return 0;
}
