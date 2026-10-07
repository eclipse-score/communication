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

//! Rust owner for a C++ `MethodInArgPtr` activity flag on the Linux x86_64 ABI.
//!
//! The C++ type is non-trivial: layout compatibility does not permit passing it by value
//! through `extern "C"`. A method bridge must use pointers and explicit construction/move/
//! destruction. This crate does not add the method runtime (communication #782).
//!
//! Rust construction exclusively borrows the element and flag until drop. A Rust move
//! transfers that borrow; drop clears the flag once and never frees the borrowed element.
//! Raw fields are private; there is no safe way to duplicate ownership or outlive either
//! borrow. The caller must not let C++ own the same flag concurrently. The non-atomic flag
//! also makes cross-thread transfer/sharing unsupported (`!Send` and `!Sync`).

use core::fmt::Debug;
use core::marker::PhantomData;

/// Move-only owner matching `{T*, bool&, size_t}` on the supported Linux x86_64 ABI.
///
/// The lifetime marker has zero size and ties both pointers to exclusive Rust borrows.
/// C++ reference representation is platform-specific and checked by native tests.
///
/// ```
/// use method_in_arg_ptr_rs::MethodInArgPtr;
/// let mut value = 42;
/// let mut active = false;
/// let owner = MethodInArgPtr::new(&mut value, &mut active, 7);
/// assert_eq!(owner.queue_position(), 7);
/// drop(owner);
/// assert!(!active);
/// assert_eq!(value, 42);
/// ```
///
/// ```compile_fail
/// use method_in_arg_ptr_rs::MethodInArgPtr;
/// let mut value = 42;
/// let mut active = false;
/// let owner = MethodInArgPtr::new(&mut value, &mut active, 0);
/// let moved = owner;
/// drop(owner); // ownership was transferred
/// drop(moved);
/// ```
///
/// ```compile_fail
/// use method_in_arg_ptr_rs::MethodInArgPtr;
/// let mut value = 42;
/// let mut active = false;
/// let owner = MethodInArgPtr::new(&mut value, &mut active, 0);
/// active = false; // exclusively borrowed until drop
/// drop(owner);
/// ```
///
/// ```compile_fail
/// use method_in_arg_ptr_rs::MethodInArgPtr;
/// let owner;
/// { let mut value = 42; let mut active = false;
///   owner = MethodInArgPtr::new(&mut value, &mut active, 0); }
/// drop(owner); // neither referent may expire first
/// ```
///
/// ```compile_fail
/// use method_in_arg_ptr_rs::MethodInArgPtr;
/// fn require_send<T: Send>() {}
/// require_send::<MethodInArgPtr<'static, i32>>();
/// ```
///
/// ```compile_fail
/// use method_in_arg_ptr_rs::MethodInArgPtr;
/// fn require_sync<T: Sync>() {}
/// require_sync::<MethodInArgPtr<'static, i32>>();
/// ```
#[repr(C)]
pub struct MethodInArgPtr<'a, T> {
    element_ptr: *mut T,
    ptr_active: *mut bool,
    queue_position: usize,
    lifetime: PhantomData<(&'a mut T, &'a mut bool)>,
}

impl<'a, T> MethodInArgPtr<'a, T> {
    /// Exclusively borrows the element and activity flag, marking the flag active.
    pub fn new(element: &'a mut T, active: &'a mut bool, queue_position: usize) -> Self {
        *active = true;
        Self {
            element_ptr: element,
            ptr_active: active,
            queue_position,
            lifetime: PhantomData,
        }
    }

    /// Identifies the call queue slot without exposing the borrowed pointers.
    pub fn queue_position(&self) -> usize {
        self.queue_position
    }
}

impl<T> Drop for MethodInArgPtr<'_, T> {
    fn drop(&mut self) {
        // SAFETY: `new` exclusively borrows a live bool for this owner's lifetime. The
        // private fields and absence of Clone/Copy prevent a second owner. Rust moves
        // do not run the source destructor. This releases only the activity flag.
        unsafe {
            *self.ptr_active = false;
        }
    }
}

impl<T> Debug for MethodInArgPtr<'_, T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("MethodInArgPtr")
            .field("queue_position", &self.queue_position)
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use test_helper_size_ffi_rs::{verify_method_in_arg_cpp, MethodInArgPtrLola};
    use test_utils_rs::*;

    #[test]
    fn int32_layout() {
        verify_size_and_align!(
            MethodInArgPtr<'_, i32>,
            MethodInArgPtrLola::get_int32(),
            "MethodInArgPtr<i32>"
        );
    }

    #[test]
    fn user_type_layout() {
        verify_size_and_align!(
            MethodInArgPtr<'_, UserType>,
            MethodInArgPtrLola::get_user_defined_type(),
            "MethodInArgPtr<UserType>"
        );
    }

    #[test]
    fn cpp_member_representation_and_move_destruction() {
        let mut value = 42;
        let mut active = false;
        let owner = MethodInArgPtr::new(&mut value, &mut active, 17);
        // SAFETY: The C++ test only reads the Rust representation and operates on its
        // own local C++ object/referents. It neither borrows nor destroys this owner.
        assert!(unsafe {
            verify_method_in_arg_cpp(
                (&raw const owner).cast(),
                owner.element_ptr.cast(),
                owner.ptr_active.cast(),
                17,
            )
        });
        let moved = owner;
        assert_eq!(moved.queue_position(), 17);
        // SAFETY: owner still exclusively holds this live flag; only a read is made.
        assert!(unsafe { *moved.ptr_active });
        drop(moved);
        assert!(!active);
        assert_eq!(value, 42);
    }

    #[test]
    fn replacement_releases_previous_flag() {
        let mut first = 1;
        let mut second = 2;
        let mut first_active = false;
        let mut second_active = false;
        let mut owner = MethodInArgPtr::new(&mut first, &mut first_active, 1);
        assert_eq!(owner.queue_position(), 1);
        owner = MethodInArgPtr::new(&mut second, &mut second_active, 2);
        assert!(!first_active);
        assert_eq!(owner.queue_position(), 2);
        drop(owner);
        assert!(!second_active);
    }
}
