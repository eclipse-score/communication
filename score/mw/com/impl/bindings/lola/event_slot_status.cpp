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
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

// Hot-path optimization: all member functions of EventSlotStatus are now defined inline in the header so that they can
// be inlined into the GetNewSamples() sample-collection loop. This translation unit is intentionally left without
// out-of-line definitions.
