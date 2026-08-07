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

/// Macro to implement the Consumer trait for a given interface ID and its events.
///
/// Generates the Consumer struct with subscribers for each event.
// TODO: This can be removed once verification is done that the new interface_producer_mixed! macro works for event-only interfaces.
#[macro_export]
macro_rules! interface_consumer {
    ($id:ident, $($event_name:ident, Event<$event_type:ty>),+$(,)?) => {
        score_com::paste::paste!  {
            pub struct [<$id Consumer>]<R: score_com::Runtime + ?Sized> {
                $(
                    pub $event_name: R::Subscriber<$event_type>,
                )+
            }

            impl<R: score_com::Runtime + ?Sized> score_com::Consumer<R> for [<$id Consumer>]<R> {
                fn new(instance_info: R::ConsumerInfo) -> Self {
                    [<$id Consumer>] {
                        $(
                            $event_name: R::Subscriber::new(
                                stringify!($event_name),
                                instance_info.clone()
                            ).expect(&format!(
                                "Failed to create subscriber for {}",
                                stringify!($event_name)
                            )),
                        )+
                    }
                }
            }
        }
    };
}

/// Generates the `{id}Consumer<R>` struct and its `Consumer<R>` trait implementation for
/// interfaces that may contain any combination of events, fields, and methods.
///
/// Field members are passed as three separate flat lists (one per tag type):
/// - `fields_notifier[name:type,...]` - `pub name: R::FieldSubscriber<T>` (subscribe/notifications)
/// - `fields_getter[name:type,...]`   - `pub name_get: R::FieldGetCaller<T>` + `get_name()` wrapper
/// - `fields_setter[name:type,...]`   - `pub name_set: R::FieldSetCaller<T>` + `set_name(val)` wrapper
///
/// A field with multiple tags (e.g. `WithGetter + WithSetter`) appears in multiple lists.
/// Method members get positional-arg wrapper functions via `_gen_method_wrapper!`.
#[doc(hidden)]
#[macro_export]
macro_rules! interface_consumer_mixed {
    (
        $id:ident,
        events[$($ev_name:ident : $ev_type:ty ,)*],
        fields_notifier[$($fin_name:ident : $fin_type:ty ,)*],
        fields_getter[$($fig_name:ident : $fig_type:ty ,)*],
        fields_setter[$($fis_name:ident : $fis_type:ty ,)*],
        methods[$($me_name:ident [$($me_arg_ty:ty),*] -> $me_ret:ty ,)*]
    ) => {
        score_com::paste::paste! {
            pub struct [<$id Consumer>]<R: score_com::Runtime + ?Sized> {
                $(
                    pub $ev_name: R::Subscriber<$ev_type>,
                )*
                $(
                    pub $fin_name: R::FieldSubscriber<$fin_type>,
                )*
                $(
                    pub [<$fig_name _get>]: R::FieldGetCaller<$fig_type>,
                )*
                $(
                    pub [<$fis_name _set>]: R::FieldSetCaller<$fis_type>,
                )*
                $(
                    pub $me_name: R::MethodCaller<($($me_arg_ty,)*), $me_ret>,
                )*
            }

            impl<R: score_com::Runtime + ?Sized> score_com::Consumer<R> for [<$id Consumer>]<R> {
                fn new(instance_info: R::ConsumerInfo) -> Self {
                    [<$id Consumer>] {
                        $(
                            $ev_name: R::Subscriber::new(
                                stringify!($ev_name),
                                instance_info.clone()
                            ).expect(&format!(
                                "Failed to create subscriber for {}",
                                stringify!($ev_name)
                            )),
                        )*
                        $(
                            $fin_name: R::FieldSubscriber::new(
                                stringify!($fin_name),
                                instance_info.clone()
                            ).expect(&format!(
                                "Failed to create field subscriber for {}",
                                stringify!($fin_name)
                            )),
                        )*
                        $(
                            [<$fig_name _get>]: <R::FieldGetCaller<$fig_type>
                                as score_com::MethodCaller<(), $fig_type, R>>::new(
                                    concat!(stringify!($fig_name), "_get"),
                                    instance_info.clone()
                                ).expect(&format!(
                                    "Failed to create field get caller for {}",
                                    stringify!($fig_name)
                                )),
                        )*
                        $(
                            [<$fis_name _set>]: <R::FieldSetCaller<$fis_type>
                                as score_com::MethodCaller<($fis_type,), $fis_type, R>>::new(
                                    concat!(stringify!($fis_name), "_set"),
                                    instance_info.clone()
                                ).expect(&format!(
                                    "Failed to create field set caller for {}",
                                    stringify!($fis_name)
                                )),
                        )*
                        $(
                            $me_name: <R::MethodCaller<($($me_arg_ty,)*), $me_ret>
                                as score_com::MethodCaller<($($me_arg_ty,)*), $me_ret, R>>::new(
                                    stringify!($me_name),
                                    instance_info.clone()
                                ).expect(&format!(
                                    "Failed to create method caller for {}",
                                    stringify!($me_name)
                                )),
                        )*
                    }
                }
            }

            // Method wrappers: positional-arg convenience functions via MethodCallInput.
            // Field get/set wrappers: async get/set via FieldGetCaller/FieldSetCaller.
            // subscribe() is available directly on the struct field for WithNotifier fields.
            impl<R: score_com::Runtime + ?Sized> [<$id Consumer>]<R> {
                $(
                    $crate::_gen_method_wrapper!($me_name ($($me_arg_ty),*) -> $me_ret);
                )*
                $(
                    /// Asynchronously get the current value of the field.
                    /// Returns a future that resolves to `Result<R::MethodReturnSample<T>>`.
                    /// Independent of subscription lifecycle.
                    pub fn [<get_ $fig_name>]<'a>(
                        &'a self,
                    ) -> impl core::future::Future<Output = score_com::Result<<R as score_com::Runtime>::MethodReturnSample<$fig_type>>> + 'a {
                        score_com::MethodCaller::invoke_with_copy(&self.[<$fig_name _get>], ())
                    }
                )*
                $(
                    /// Asynchronously set the value of the field.
                    /// Returns a future that resolves to `Result<R::MethodReturnSample<T>>`
                    /// containing the confirmed field value from the producer.
                    /// Independent of subscription lifecycle.
                    pub fn [<set_ $fis_name>]<'a>(
                        &'a self,
                        value: $fis_type,
                    ) -> impl core::future::Future<Output = score_com::Result<<R as score_com::Runtime>::MethodReturnSample<$fis_type>>> + 'a {
                        score_com::MethodCaller::invoke_with_copy(&self.[<$fis_name _set>], (value,))
                    }
                )*
            }
        }
    };
}

/// Entry-point wrapper generator.
/// Every generated wrapper returns `impl Future<Output = score_com::Result<R::MethodReturnSample<Return>>> + '_`.
///
/// # Generated call sites
/// ```text
/// consumer.method(val).await  - copy path   - val: ArgType
/// consumer.method(ptr).await  - zero-copy   - ptr: MethodInArgPtr<ArgType>
/// ```
#[doc(hidden)]
#[macro_export]
macro_rules! _gen_method_wrapper {
    // 0 args - invoke_with_copy directly; no zero-copy path (nothing to allocate).
    // This is for kind of `get` methods that take no arguments and return a value.
    ($me_name:ident () -> $me_ret:ty) => {
        pub fn $me_name<'a>(&'a self) -> impl core::future::Future<Output = score_com::Result<<R as score_com::Runtime>::MethodReturnSample<$me_ret>>> + 'a {
            score_com::MethodCaller::invoke_with_copy(&self.$me_name, ())
        }
    };
    // 1-N args - delegate to the self-counting recursive macro.
    ($me_name:ident ($($t:ty),+) -> $me_ret:ty) => {
        $crate::_gen_method_wrapper_collect!(
            $me_name -> $me_ret ;
            @counter[]
            @acc[]
            @types[$($t),+]
        );
    };
}

/// Recursive macro for `_gen_method_wrapper!`.
///
/// Self-counting: instead of zipping the method's positional type list against a
/// pre-defined pool of `(arg_name, generic_name)` identifiers, this recursive macro synthesizes
/// a fresh, unique `(argN : _AN : TypeN)` triplet at each recursion step directly from a
/// growing counter of `n` marker tokens (via `paste!`), then calls
/// `_gen_method_wrapper_body!` once the type list is exhausted.
///
/// This mirrors the self-contained recursion used by `impl_all_arities!` in
/// `method_arities.rs`: there is no separate pool to keep in sync, and no fixed
/// argument-count limit - any arity supported by `method_arities.rs` works automatically.
#[doc(hidden)]
#[macro_export]
macro_rules! _gen_method_wrapper_collect {
    // Base: all types consumed - emit the function via the body macro.
    (
        $me_name:ident -> $me_ret:ty ;
        @counter[$($n:tt)*]
        @acc[$($acc:tt),*]
        @types[]
    ) => {
        $crate::_gen_method_wrapper_body!($me_name -> $me_ret ; [$($acc),*]);
    };

    // Step: consume one type, grow the counter by one `n`, and synthesize a fresh
    // (param, generic) identifier pair from the counter via `paste!`.
    (
        $me_name:ident -> $me_ret:ty ;
        @counter[$($n:tt)*]
        @acc[$($acc:tt),*]
        @types[$t:ty $(, $rest_t:ty)*]
    ) => {
        score_com::paste::paste! {
            $crate::_gen_method_wrapper_collect!(
                $me_name -> $me_ret ;
                @counter[$($n)* n]
                @acc[$($acc,)* ([<arg $($n)*>] : [<_A $($n)*>] : $t)]
                @types[$($rest_t),*]
            );
        }
    };
}

/// Generates the wrapper function from an accumulated list of `(argN : _AN : TypeN)`.
///
/// This generates a wrapper function template.
/// All arities use this one arm - the function body is written once, not duplicated per arity.
/// Called by `_gen_method_wrapper_collect!` after it has built the full triplet list.
///
/// The generated function returns `impl Future<Output = score_com::Result<R::MethodReturnSample<$me_ret>>> + 'a` so callers
/// can `.await` the method call, e.g. `consumer.method_name(arg0).await?`.
#[doc(hidden)]
#[macro_export]
macro_rules! _gen_method_wrapper_body {
    ($me_name:ident -> $me_ret:ty ; [$(($p:ident : $g:ident : $c:ty)),+]) => {
        pub fn $me_name<'a, $($g),+>(
            &'a self,
            $($p: $g),+
        ) -> impl core::future::Future<Output = score_com::Result<<R as score_com::Runtime>::MethodReturnSample<$me_ret>>> + 'a
        where
            ($($g,)+): score_com::MethodCallInput<($($c,)+), $me_ret, R>,
            R::MethodCaller<($($c,)+), $me_ret>:
                score_com::MethodCaller<($($c,)+), $me_ret, R>,
        {
            score_com::MethodCallInput::invoke(($($p,)+), &self.$me_name)
        }
    };
}
