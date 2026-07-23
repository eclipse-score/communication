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

//! Arity-parameterised blanket impls for method argument tuples.
//!
//! # Single configuration point
//!
//! To raise (or lower) the maximum number of arguments a method may have, edit the
//! `impl_all_arities!` invocation at the bottom of this file.  Add one
//! more `(TypeIdent, arg_ident)` pair per additional argument.  Everything else -
//! `Reloc`, `CommData`, `MethodArgs`, `MethodArgsAllocate`, `MethodCallInput` (zero-copy
//! path), and `MethodHandlerCall` - is generated using macros.
//!
//! `_gen_method_wrapper!` in `interface_macros.rs` self-generates its argument
//! identifiers via a counting recursive macro, so it has no separate limit to keep in
//! sync - raising the arity here is the only change needed.
//! (I don't think this many arguments will support by clippy linting, 
//!  so we may need to reduce the limit to 8 or 10 in the future based on project clippy linting rules.)
//!
//! # Arity 0 special case
//!
//! Arity 0 (`()`) is handled separately in `com_api_method.rs` because the zero-tuple
//! has no positional variables to destructure.  This macro covers arities **1 and above**.
//!
//! # How the recursive macro works
//!
//! The macro maintains two accumulated token lists in parallel:
//! - **Type vars** `[T1, T2, …]` - used as generic parameters in trait impls.
//! - **Arg names** `[a0, a1, …]` - used for positional destructuring inside
//!   `MethodHandlerCall::call` and `MethodCallInput::invoke`.
//!
//! At each step the next `(TypeIdent, arg_ident)` pair is peeled from the input, the
//! two accumulated lists grow by one, all six impls for the new arity are emitted, and
//! the recursion continues with the extended lists.

use crate::{
    CommData, MethodArgs, MethodArgsAllocate, MethodCallInput, MethodCaller, MethodHandlerCall,
    MethodInArgAllocator, MethodInArgPtr, Reloc, Result, Runtime,
};
use core::future::Future;

/// Internal recursive macro.  Do not invoke directly - use `impl_all_arities!` below.
#[doc(hidden)]
macro_rules! impl_all_arities {
    ( $( ($T:ident, $a:ident) ),+ $(,)? ) => {
        impl_all_arities!(@step [] [] [$( ($T, $a) ),+]);
    };

    (@step [$($T:ident),*] [$($a:ident),*] []) => {};

    (
        @step [$($T:ident),*] [$($a:ident),*]
        [($nextT:ident, $nextA:ident) $(, ($restT:ident, $restA:ident))*]
    ) => {
        
        unsafe impl<$($T: Reloc,)* $nextT: Reloc> Reloc for ($($T,)* $nextT,) {}

        impl<$($T: CommData,)* $nextT: CommData> CommData for ($($T,)* $nextT,) {
            // Placeholder ID - the tuple structure itself serves as the identifier.
            const ID: &'static str = stringify!(($($T,)* $nextT,));
        }

        impl<$($T: CommData,)* $nextT: CommData> MethodArgs for ($($T,)* $nextT,) {
            type PtrTuple = ($( MethodInArgPtr<$T>, )* MethodInArgPtr<$nextT>,);
        }

        impl<$($T: CommData,)* $nextT: CommData, _Alloc: MethodInArgAllocator>
            MethodArgsAllocate<_Alloc> for ($($T,)* $nextT,)
        {
            type UninitTuple = (
                $( _Alloc::MethodInArgMaybeUninit<$T>, )*
                _Alloc::MethodInArgMaybeUninit<$nextT>,
            );

            fn alloc_uninit(allocator: &_Alloc) -> Self::UninitTuple {
                ($( allocator.allocate::<$T>(), )* allocator.allocate::<$nextT>(),)
            }
        }

        // Accepts a tuple of `MethodInArgPtr<T>` values and dispatches to
        // `invoke_zero_copy`. The copy path is already covered by the blanket impl
        // in `com_api_method.rs` and does not need to be repeated here.
        impl<$($T: CommData,)* $nextT: CommData, Return: CommData, R: Runtime + ?Sized>
            MethodCallInput<($($T,)* $nextT,), Return, R>
            for ($( MethodInArgPtr<$T>, )* MethodInArgPtr<$nextT>,)
        where
            R::MethodCaller<($($T,)* $nextT,), Return>:
                MethodCaller<($($T,)* $nextT,), Return, R>,
        {
            fn invoke<'a>(
                self,
                caller: &'a R::MethodCaller<($($T,)* $nextT,), Return>,
            ) -> impl Future<Output = Result<Return>> + 'a
            where
                R::MethodCaller<($($T,)* $nextT,), Return>:
                    MethodCaller<($($T,)* $nextT,), Return, R> + 'a,
            {
                // Destructure with positional arg names, then reconstruct the ptr tuple.
                #[allow(non_snake_case)]
                let ($($a,)* $nextA,) = self;
                <R::MethodCaller<($($T,)* $nextT,), Return> as
                    MethodCaller<($($T,)* $nextT,), Return, R>>::invoke_zero_copy(
                    caller,
                    ($($a,)* $nextA,),
                )
            }
        }

        // Maps a plain `Fn(T1, T2, …) -> Return` closure to the tuple-based call
        // convention used by the runtime.
        impl<_F, $($T,)* $nextT, Return> MethodHandlerCall<($($T,)* $nextT,), Return> for _F
        where
            _F: Fn($($T,)* $nextT,) -> Return + Send + Sync + 'static,
        {
            fn call(&self, args: ($($T,)* $nextT,)) -> Return {
                #[allow(non_snake_case)]
                let ($($a,)* $nextA,) = args;
                (self)($($a,)* $nextA,)
            }
        }

        impl_all_arities!(
            @step [$($T,)* $nextT] [$($a,)* $nextA]
            [$( ($restT, $restA) ),*]
        );
    };
}

// Single configuration point
//
// To raise the maximum method argument count:
//   1. Add one more `(TypeIdent, arg_ident)` pair below.
//   2. That's it - all six trait impls are generated automatically.
//
// `_gen_method_wrapper!` (`interface_macros.rs`) has no fixed arity limit of its own,
// so raising the limit here is the only change needed.
//
// Current limit: 8 arguments.
impl_all_arities!(
    (T1, a0),
    (T2, a1),
    (T3, a2),
    (T4, a3),
    (T5, a4),
    (T6, a5),
    (T7, a6),
    (T8, a7),
);
