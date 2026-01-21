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

pub type SlotIndexType = u16;
pub type TransactionLogIndex = u8;

#[repr(C)]
pub struct ControlSlotType {
    _dummy: [u8; 0],
}

#[repr(C)]
pub struct CxxOptional<T> {
    _data: T,
    _engaged: bool,
}

#[repr(C)]
pub struct EventDataControl {
    _dummy: [u8; 0],
}

#[repr(C)]
pub struct BlankBinding {
    _data: [u8; 0],
}
