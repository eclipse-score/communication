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

// Test fixture: a serialization library that exports its entry points but no ABI descriptor, like a library that
// was built before the descriptor existed. mw::com must refuse to use it.
extern "C" {

void* CreateSerializer_0123_4567() noexcept
{
    return nullptr;
}

void DestroySerializer(void*) noexcept {}

}  // extern "C"
