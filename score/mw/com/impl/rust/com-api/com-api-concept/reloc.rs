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

/// This trait shall ensure that we can safely use an instance of the implementing type across
/// address boundaries. This property may be violated by the following circumstances:
/// - usage of pointers to other members of the struct itself (akin to !Unpin structs)
/// - usage of Rust pointers or references to other data
///
/// This can be trivially achieved by not using any sort of reference. In case a reference (either
/// to self or to other data) is required, the following options exist:
/// - Use indices into other data members of the same structure
/// - Use offset pointers _to the same memory chunk_ that point to different (external) data
///
/// # Safety
///
/// Since it is yet to be proven whether this trait can be implemented safely (assumption is: no) it
/// is unsafe for now. The expectation is that very few users ever need to implement this manually.
pub unsafe trait Reloc {}

unsafe impl Reloc for () {}
unsafe impl Reloc for u8 {}
unsafe impl Reloc for u16 {}
unsafe impl Reloc for u32 {}
unsafe impl Reloc for u64 {}
unsafe impl Reloc for i8 {}
unsafe impl Reloc for i16 {}
unsafe impl Reloc for i32 {}
unsafe impl Reloc for i64 {}
unsafe impl Reloc for f32 {}
unsafe impl Reloc for f64 {}
unsafe impl Reloc for bool {}
unsafe impl Reloc for char {}
unsafe impl Reloc for usize {}
unsafe impl Reloc for u128 {}
unsafe impl Reloc for i128 {}
unsafe impl Reloc for isize {}

// Arrays
unsafe impl<T: Reloc, const N: usize> Reloc for [T; N] {}

// MaybeUninit
unsafe impl<T: Reloc> Reloc for core::mem::MaybeUninit<T> {}

// Tuples (up to 5 elements)
unsafe impl<T1: Reloc> Reloc for (T1,) {}
unsafe impl<T1: Reloc, T2: Reloc> Reloc for (T1, T2) {}
unsafe impl<T1: Reloc, T2: Reloc, T3: Reloc> Reloc for (T1, T2, T3) {}
unsafe impl<T1: Reloc, T2: Reloc, T3: Reloc, T4: Reloc> Reloc for (T1, T2, T3, T4) {}
unsafe impl<T1: Reloc, T2: Reloc, T3: Reloc, T4: Reloc, T5: Reloc> Reloc for (T1, T2, T3, T4, T5) {}
