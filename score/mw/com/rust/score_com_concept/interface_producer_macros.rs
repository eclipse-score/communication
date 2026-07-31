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

// Root interface!, interface_common!, _field_split_tags!, _interface_collect_members!,
// tag structs, and all tests have been moved to interface_macros.rs.

/// This is Event specific.
/// Macro to implement the Producer and OfferedProducer traits for
/// a given interface ID and its events.
/// Generates Producer and OfferedProducer structs with publishers for each event.
// TODO: This can be removed once verification is done that the new interface_producer_mixed! macro works for event-only interfaces.
#[macro_export]
macro_rules! interface_producer {
    ($id:ident, $($event_name:ident, Event<$event_type:ty>),+$(,)?) => {
        score_com::paste::paste!  {
            pub struct [<$id Producer>]<R: score_com::Runtime + ?Sized> {
                _runtime: core::marker::PhantomData<R>,
                instance_info: R::ProviderInfo,
            }

            pub struct [<$id OfferedProducer>]<R: score_com::Runtime + ?Sized> {
                $(
                    pub $event_name: R::Publisher<$event_type>,
                )+
                instance_info: R::ProviderInfo,
            }

            impl<R: score_com::Runtime + ?Sized> score_com::Producer<R> for [<$id Producer>]<R> {
                type Interface = [<$id Interface>];
                type OfferedProducer = [<$id OfferedProducer>]<R>;
                fn offer(self) -> score_com::Result<Self::OfferedProducer> {
                    let offered = [<$id OfferedProducer>] {
                        $(
                            $event_name: R::Publisher::new(
                                stringify!($event_name),
                                self.instance_info.clone()
                            ).expect(&format!(
                                "Failed to create publisher for {}",
                                stringify!($event_name)
                            )),
                        )+
                        instance_info: self.instance_info.clone(),
                    };
                    // Offer the service instance to make it discoverable
                    self.instance_info.offer_service()?;
                    Ok(offered)
                }

                fn new(instance_info: R::ProviderInfo) -> score_com::Result<Self> {
                    Ok([<$id Producer>] {
                        _runtime: core::marker::PhantomData,
                        instance_info,
                    })
                }
            }

            impl<R: score_com::Runtime + ?Sized> score_com::OfferedProducer<R>
                for [<$id OfferedProducer>]<R> {
                type Interface = [<$id Interface>];
                type Producer = [<$id Producer>]<R>;
                fn unoffer(self) -> score_com::Result<Self::Producer> {
                    let producer = [<$id Producer>] {
                        _runtime: core::marker::PhantomData,
                        instance_info: self.instance_info.clone(),
                    };
                    // Stop offering the service instance to withdraw it from system availability
                    self.instance_info.stop_offer_service()?;
                    Ok(producer)
                }
            }
        }
    };
}

/// Generates `{id}Producer<R>`, `{id}OfferedProducer<R>`, and all trait implementations for
/// interfaces that may contain any combination of events, fields, and methods.
///
/// # Design
/// - Event publishers (`R::Publisher<T>`) are created *lazily during `_offer_internal()`*
///   so they are only present on the `OfferedProducer`.
/// - Field publishers (`R::FieldPublisher<T>`) are created eagerly in `Producer::new()` and
///   moved into `OfferedProducer` when the service is offered.
/// - Method handlers (`R::MethodHandler<Args, Ret>`) likewise created eagerly and moved.
///
/// When the interface has at least one field or method member, the `Producer` struct derives
/// `TypeStateValidator` which generates the `.init()` entry point and the `update_*` /
/// `register_set_handler_*` / `register_*_handler` chain required before `offer()`.
///
/// When the interface has only events (no fields, no methods), a plain `offer()` is generated
/// directly (matching the existing event-only pattern).
#[doc(hidden)]
#[macro_export]
macro_rules! interface_producer_mixed {
    // Event-only specialisation (no fields, no methods):
    // plain offer() without type-state validation - identical to interface_producer!
    (
        $id:ident,
        events[$($ev_name:ident : $ev_type:ty ,)+],
        fields[],
        fields_setter[],
        fields_getter[],
        methods[]
    ) => {
        $crate::interface_producer!($id, $($ev_name, Event<$ev_type>),+);
    };

    // General case: at least one field or method (or both), possibly with events too.
    (
        $id:ident,
        events[$($ev_name:ident : $ev_type:ty ,)*],
        fields[$($fi_name:ident : $fi_type:ty ,)*],
        fields_setter[$($fis_name:ident : $fis_type:ty ,)*],
        fields_getter[$($fig_name:ident : $fig_type:ty ,)*],
        methods[$($me_name:ident [$($me_arg_ty:ty),*] -> $me_ret:ty ,)*]
    ) => {
        score_com::paste::paste! {
            // Producer struct - derives TypeStateValidator for compile-time `offer()`.
            // Fields: FieldPublisher per field + MethodHandler per method.
            // Event publishers are NOT stored here they are created during _offer_internal().
            // #[field_setter_list] and #[field_getter_list] are derive helper attributes introduced
            // by TypeStateValidator. They tell it which fields have WithSetter / WithGetter tags
            // so only those fields get the corresponding type-state handler steps.
            #[derive($crate::score_com_macros::TypeStateValidator)]
            #[field_setter_list($($fis_name,)*)]
            #[field_getter_list($($fig_name,)*)]
            pub struct [<$id Producer>]<R: score_com::Runtime + ?Sized> {
                $(
                    $fi_name: R::FieldPublisher<$fi_type>,
                )*
                $(
                    $me_name: R::MethodHandler<($($me_arg_ty,)*), $me_ret>,
                )*
                pub instance_info: R::ProviderInfo,
            }

            // OfferedProducer struct - contains event publishers (created on offer),
            // plus the moved field publishers and method handlers from Producer.
            pub struct [<$id OfferedProducer>]<R: score_com::Runtime + ?Sized> {
                $(
                    pub $ev_name: R::Publisher<$ev_type>,
                )*
                $(
                    pub $fi_name: R::FieldPublisher<$fi_type>,
                )*
                $(
                    $me_name: R::MethodHandler<($($me_arg_ty,)*), $me_ret>,
                )*
                instance_info: R::ProviderInfo,
            }

            // Internal implementation - called by the TypeStateValidator's offer() after all
            // states have been validated at compile time.
            impl<R: score_com::Runtime + ?Sized> [<$id Producer>]<R> {
                #[doc(hidden)]
                pub fn _offer_internal(
                    self,
                ) -> score_com::Result<[<$id OfferedProducer>]<R>> {
                    let offered = [<$id OfferedProducer>] {
                        $(
                            $ev_name: R::Publisher::new(
                                stringify!($ev_name),
                                self.instance_info.clone()
                            ).expect(&format!(
                                "Failed to create publisher for {}",
                                stringify!($ev_name)
                            )),
                        )*
                        $(
                            $fi_name: self.$fi_name,
                        )*
                        $(
                            $me_name: self.$me_name,
                        )*
                        instance_info: self.instance_info.clone(),
                    };
                    self.instance_info.offer_service()?;
                    Ok(offered)
                }
            }

            // We can not remove the offer method from the Producer trait, but we can override it to panic with a clear message.
            // Also adding compiler warning or error for this is not possible, we will rely on documentation and panic.
            // if user call this directly, then it will panic and it is against the intended usage of the APIs.
            // TODO: Need to think about this more, when we have more complex interface with mixed types.
            // Also update the documentation for this, so user should not call offer() directly from Producer struct.
            impl<R: score_com::Runtime + ?Sized> score_com::Producer<R> for [<$id Producer>]<R> {
                type Interface = [<$id Interface>];
                type OfferedProducer = [<$id OfferedProducer>]<R>;

                fn offer(self) -> score_com::Result<Self::OfferedProducer> {
                    panic!(
                        "ERROR: Cannot call {producer}.offer() directly.\n\
                         All fields must be initialized and all handlers must be registered first.\n\
                         Correct usage: producer.init()\
                             .update_<field>(&val)?\
                             .register_set_handler_<field>(|v| {{ ... }})\
                             .register_<method>_handler(|args| {{ ... }})\
                             .offer()?",
                        producer = stringify!([<$id Producer>])
                    )
                }

                fn new(instance_info: R::ProviderInfo) -> score_com::Result<Self> {
                    Ok([<$id Producer>] {
                        $(
                            $fi_name: R::FieldPublisher::new(
                                stringify!($fi_name),
                                instance_info.clone()
                            )?,
                        )*
                        $(
                            $me_name: <R::MethodHandler<($($me_arg_ty,)*), $me_ret>
                                as score_com::MethodHandler<($($me_arg_ty,)*), $me_ret, R>>::new(
                                    stringify!($me_name),
                                    instance_info.clone()
                                )?,
                        )*
                        instance_info,
                    })
                }
            }

            // OfferedProducer trait impl - unoffer() stops the service and returns the Producer.
            impl<R: score_com::Runtime + ?Sized> score_com::OfferedProducer<R>
                for [<$id OfferedProducer>]<R>
            {
                type Interface = [<$id Interface>];
                type Producer = [<$id Producer>]<R>;

                fn unoffer(self) -> score_com::Result<Self::Producer> {
                    self.instance_info.stop_offer_service()?;
                    Ok([<$id Producer>] {
                        $(
                            $fi_name: self.$fi_name,
                        )*
                        $(
                            $me_name: self.$me_name,
                        )*
                        instance_info: self.instance_info,
                    })
                }
            }
        }
    };
}
