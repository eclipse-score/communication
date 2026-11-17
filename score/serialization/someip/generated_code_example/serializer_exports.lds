/*******************************************************************************
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
 *******************************************************************************/

/* Only the FFI entry points resolved by mw::com via dlsym are part of the library ABI:
 * CreateSerializer_<structural-hash>_<attribute-hash> and DestroySerializer.
 * Everything else (C++ symbols, helpers) stays hidden. */
{
  global:
    CreateSerializer_*;
    DestroySerializer;
  local:
    *;
};
