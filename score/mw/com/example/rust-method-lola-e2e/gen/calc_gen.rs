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

//! The Calc interface of the cross-process method test: methods of every shape.
//! `calc_gen.h`/`calc_gen.cpp` are its C++ side.

// The traits `interface!` may need in scope depend on the member kinds.
#[allow(unused_imports)]
use score_com::{interface, CommData, FieldPublisher, ProviderInfo, Publisher, Reloc, Subscriber};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc, CommData)]
#[repr(C)]
#[comm_data(id = "Operands")]
pub struct Operands {
    pub a: i32,
    pub b: i32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc, CommData)]
#[repr(C)]
#[comm_data(id = "Total")]
pub struct Total {
    pub value: i32,
}

/// Mixed sizes and alignments; must match `calc_gen.h`'s `Wide` byte for byte.
#[derive(Debug, Clone, Copy, PartialEq, Reloc, CommData)]
#[repr(C)]
#[comm_data(id = "Wide")]
pub struct Wide {
    pub a: u8,
    pub b: u64,
    pub c: u16,
    pub d: f64,
}

interface!(
    interface Calc {
        Id = "CalcInterface",
        add: Method(Operands) -> Total,
        mix: Method(Wide, Wide) -> Wide,
        ping: Method() -> Total,
        reset: Method(Total) -> (),
        last: Method() -> Total,
        slow: Method(Total) -> Total,
    }
);
