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
#[doc(hidden)]
pub trait TypeInfo {
    fn get_size() -> u31;
}

#[doc(hidden)]
pub mod proxy_bridge {
    pub use proxy_bridge_rs::*;
    pub use sample_ptr_rs::SamplePtr;
}

pub use paste::paste;

#[doc(hidden)]
pub mod skeleton_bridge {
    pub use skeleton_bridge_rs::*;
}

#[doc(hidden)]
pub mod common_types {
    pub use common::*;
}

#[macro_export]
macro_rules! import_type {
    ($uid:ident, $ctype:ty) => {
        #[allow(non_snake_case)]
        mod $uid {
            unsafe extern "C" {
                #[link_name=concat!("mw_com_gen_SamplePtr_", stringify!($uid), "_get")]
                pub unsafe fn get(
                    sample_ptr: *const $crate::proxy_bridge::SamplePtr<$ctype>,
                ) -> *const $ctype;
                #[link_name=concat!("mw_com_gen_ProxyEvent_", stringify!($uid), "_get_new_samples")]
                pub unsafe fn get_new_samples(
                    proxy_event: *mut $crate::proxy_bridge::NativeProxyEvent<$ctype>,
                    callback: *const $crate::proxy_bridge::FatPtr,
                ) -> u32;
                #[link_name=concat!("mw_com_gen_SamplePtr_", stringify!($uid), "_delete")]
                pub unsafe fn delete(sample_ptr: *mut $crate::proxy_bridge::SamplePtr<$ctype>);
                #[link_name=concat!("mw_com_gen_", stringify!($uid), "_get_size")]
                pub safe fn get_size() -> u32;
                #[link_name=concat!("mw_com_gen_SkeletonEvent_", stringify!($uid), "_send")]
                pub unsafe fn send(
                    skeleton_event: *mut $crate::skeleton_bridge::NativeSkeletonEvent<$ctype>,
                    sample: *const $ctype,
                ) -> bool;
            }
        }

        impl $crate::proxy_bridge::EventOps for $ctype {
            fn get_sample_ref(ptr: &$crate::proxy_bridge::SamplePtr<Self>) -> &Self {
                unsafe {
                    $uid::get(ptr).as_ref().expect("Received null ptr from dereferenced sample_ptr")
                }
            }

            fn delete_sample_ptr(mut ptr: $crate::proxy_bridge::SamplePtr<Self>) {
                unsafe {
                    $uid::delete(&mut ptr);
                }
            }

            unsafe fn get_new_samples(
                proxy_event: *mut $crate::proxy_bridge::NativeProxyEvent<Self>,
                mut callback: impl FnMut(*mut $crate::proxy_bridge::SamplePtr<Self>),
            ) -> usize {
                let dyn_callback: &mut dyn FnMut(*mut $crate::proxy_bridge::SamplePtr<Self>) = &mut callback;
                unsafe {
                    let fat_ptr: $crate::proxy_bridge::FatPtr = std::mem::transmute(dyn_callback);
                    $uid::get_new_samples(proxy_event, &fat_ptr) as usize
                }
            }
        }

        impl $crate::TypeInfo for $ctype {
            fn get_size() -> u32 {
                $uid::get_size()
            }
        }

        impl $crate::skeleton_bridge::SkeletonOps for $ctype {
            fn send(
                &self,
                event: *mut $crate::skeleton_bridge::NativeSkeletonEvent<Self>,
            ) -> $crate::common_types::Result<()> {
                // SAFETY: calling FFI functionalities
                if unsafe { $uid::send(event, self as *const _) } {
                    Ok(())
                } else {
                    Err(())
                }
            }
        }

        $crate::paste! {
            #[no_mangle]
            extern "C" fn [<mw_com_impl_call_dyn_ref_fnmut_sample_ $uid>](
                ptr: *const $crate::proxy_bridge::FatPtr,
                sample_ptr: *mut $crate::proxy_bridge::SamplePtr<$ctype>) {
                let callable: &mut dyn FnMut(_) = unsafe { std::mem::transmute(*ptr) };
                callable(sample_ptr);
            }
        }
    };
}

#[macro_export]
macro_rules! per_event_module {
    ($uid:ident, $(($ev_name:ident: $ev_ty:ty)),*) => {
        $(pub(crate) mod $ev_name {
            unsafe extern "C" {
                #[link_name=concat!("mw_com_gen_ProxyWrapperClass_", stringify!($uid), "_", stringify!($ev_name), "_get")]
                pub fn get_proxy(proxy: *mut $crate::proxy_bridge::ProxyWrapperClass) -> *mut $crate::proxy_bridge::NativeProxyEvent<$ev_ty>;
                #[link_name=concat!("mw_com_gen_SkeletonWrapperClass_", stringify!($uid), "_", stringify!($ev_name), "_get")]
                pub fn get_skeleton(proxy: *mut $crate::skeleton_bridge::SkeletonWrapperClass) -> *mut $crate::skeleton_bridge::NativeSkeletonEvent<$ev_ty>;
            }
        })*
    }
}

#[macro_export]
macro_rules! proxy_event_fn {
    ($(($ev_name:ident: $ev_ty:ty)),*) => {
        $(fn $ev_name(manager: $crate::proxy_bridge::ProxyManager<Proxy>) -> $crate::proxy_bridge::ProxyEvent<$ev_ty, $crate::proxy_bridge::ProxyManager<Proxy>> {
            let sample_size = <$ev_ty as $crate::TypeInfo>::get_size();
            assert_eq!(std::mem::size_of::<$ev_ty>(), sample_size as usize,
                       "Sample sizes differ, maybe the definitions between C++ and Rust diverged? Aborting.");
            unsafe {
                $crate::proxy_bridge::ProxyEvent::new(manager, stringify!($ev_name), |proxy| ffi::$ev_name::get_proxy(proxy.get_native_proxy()))
            }
        })*
    }
}

#[macro_export]
macro_rules! import_interface {
    ($uid:ident, $mod_name:ident, { $($ev_name:ident: Event<$ev_ty:ty>),* }) => {
        #[allow(non_snake_case)]
        pub mod $mod_name {
            mod ffi {
                unsafe extern "C" {
                    #[link_name=concat!("mw_com_gen_ProxyWrapperClass_", stringify!($uid), "_create")]
                    pub safe fn create(handle: &$crate::proxy_bridge::HandleType) -> *mut $crate::proxy_bridge::ProxyWrapperClass;
                    #[link_name=concat!("mw_com_gen_ProxyWrapperClass_", stringify!($uid), "_delete")]
                    pub unsafe fn delete(proxy: *mut $crate::proxy_bridge::ProxyWrapperClass);
                    #[link_name=concat!("mw_com_gen_SkeletonWrapperClass_", stringify!($uid), "_delete")]
                    pub unsafe fn delete_skeleton(skeleton: *mut $crate::skeleton_bridge::SkeletonWrapperClass);
                    #[link_name=concat!("mw_com_gen_SkeletonWrapperClass_", stringify!($uid), "_create")]
                    pub unsafe fn create_skeleton(instance_specifier: *const $crate::proxy_bridge::NativeInstanceSpecifier) -> *mut $crate::skeleton_bridge::SkeletonWrapperClass;
                    #[link_name=concat!("mw_com_gen_SkeletonWrapperClass_", stringify!($uid), "_offer")]
                    pub fn offer(skeleton: *mut $crate::skeleton_bridge::SkeletonWrapperClass) -> bool;
                    #[link_name=concat!("mw_com_gen_SkeletonWrapperClass_", stringify!($uid), "_stop_offer")]
                    pub fn stop_offer(skeleton: *mut $crate::skeleton_bridge::SkeletonWrapperClass);
                }
                $crate::per_event_module!($uid, $(($ev_name: $ev_ty)),*);
            }

            /// Generated proxy. This struct keeps all events when a service is first instantiated. The
            /// underlying proxy on C++ side will be alive as long as at least one event is alive. This
            /// is done by keeping a reference from every event to the proxy adapter. If the last event
            /// is dropped, the proxy will be destroyed as well.
            ///
            /// Therefore, you may destructure it and hand the events to different parts of your program
            /// if you wish to do so.
            pub struct Proxy {
                $(pub $ev_name: $crate::proxy_bridge::ProxyEvent<$ev_ty, $crate::proxy_bridge::ProxyManager<Proxy>>),*
            }

            impl $crate::proxy_bridge::ProxyOps for Proxy {
                fn create(handle: &$crate::proxy_bridge::HandleType) -> *mut $crate::proxy_bridge::ProxyWrapperClass {
                    ffi::create(handle)
                }

                unsafe fn delete(proxy: *mut $crate::proxy_bridge::ProxyWrapperClass) {
                    unsafe {
                        ffi::delete(proxy);
                    }
                }
            }

            pub struct SkeletonLifecycleWrapper {
                skeleton_wrapper: *mut $crate::skeleton_bridge::SkeletonWrapperClass,
            }

            impl Drop for SkeletonLifecycleWrapper {
                fn drop(&mut self) {
                    // SAFETY: calling FFI functionalities
                    unsafe {
                        ffi::delete_skeleton(self.skeleton_wrapper);
                    }
                }
            }

            pub struct Events<S: $crate::skeleton_bridge::OfferState> {
                $(pub $ev_name: $crate::skeleton_bridge::SkeletonEvent<$ev_ty, S, SkeletonLifecycleWrapper>),*
            }

            pub struct Skeleton<S: $crate::skeleton_bridge::OfferState> {
                skeleton: std::sync::Arc<SkeletonLifecycleWrapper>,
                pub events: Events<S>,
            }

            impl Skeleton<$crate::skeleton_bridge::UnOffered> {
                pub fn new(
                    instance_specifier: &$crate::proxy_bridge::InstanceSpecifier,
                ) -> $crate::common_types::Result<Self> {
                    // SAFETY: calling FFI functionalities
                    unsafe {
                        let skeleton_wrapper = ffi::create_skeleton(instance_specifier.as_native());
                        let skeleton = std::sync::Arc::new(SkeletonLifecycleWrapper { skeleton_wrapper });
                        let events = Events {
                            $($ev_name: $crate::skeleton_bridge::SkeletonEvent::new(
                                ffi::$ev_name::get_skeleton(skeleton_wrapper),
                                skeleton.clone())),*
                        };

                        Ok(Self { skeleton, events })
                    }
                }

                pub fn offer_service(
                    self,
                ) -> $crate::common_types::Result<Skeleton<$crate::skeleton_bridge::Offered>> {
                    // SAFETY: calling FFI functionalities
                    unsafe {
                        if ffi::offer(self.skeleton.skeleton_wrapper) {
                            Ok(Skeleton {
                                skeleton: self.skeleton,
                                events: Events {
                                    $($ev_name: $crate::skeleton_bridge::SkeletonEvent::offer(self.events.$ev_name)),*
                                },
                            })
                        } else {
                            Err(())
                        }
                    }
                }
            }

            impl Skeleton<$crate::skeleton_bridge::Offered> {
                pub fn stop_offer_service(self) -> Skeleton<$crate::skeleton_bridge::UnOffered> {
                    // SAFETY: calling FFI functionalities
                    unsafe {
                        ffi::stop_offer(self.skeleton.skeleton_wrapper);
                        Skeleton {
                            skeleton: self.skeleton,
                            events: Events {
                                $($ev_name: $crate::skeleton_bridge::SkeletonEvent::stop_offer(self.events.$ev_name)),*
                            },
                        }
                    }
                }
            }

            impl Proxy {
                $crate::proxy_event_fn!($(($ev_name: $ev_ty)),*);

                /// Create a new instance of a proxy, connected to the service instance represented by the
                /// handle.
                pub fn new(handle: &$crate::proxy_bridge::HandleType) -> Result<Self, ()> {
                    let proxy_manager = $crate::proxy_bridge::ProxyManager::<Proxy>::new(handle)?;
                    Ok(Self {
                        $($ev_name: Self::$ev_name(proxy_manager.clone())),*
                    })
                }
            }
        }
    }
}
