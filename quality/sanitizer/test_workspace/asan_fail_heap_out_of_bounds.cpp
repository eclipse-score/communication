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
// This code snippet is copied from https://github.com/google/sanitizers/wiki/AddressSanitizerExampleHeapOutOfBounds
int main(int argc, char** argv)
{
    int* array = new int[100];
    array[0] = 0;
    int res = array[argc + 100];  // BOOM
    delete[] array;
    return res;
}
