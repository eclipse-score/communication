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
    fn get_size() -> u32;
}

#[doc(hidden)]
pub mod proxy_bridge {
    pub use proxy_bridge_rs::*;
    pub use sample_ptr_rs::SamplePtr;
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
                #[link_name=concat!("mw_com_gen_ProxyEvent_", stringify!($uid), "_get_new_sample")]
                pub unsafe fn get_new_sample(
                    proxy_event: *mut $crate::proxy_bridge::NativeProxyEvent<$ctype>,
                    sample_out: *mut $crate::proxy_bridge::SamplePtr<$ctype>,
                ) -> bool;
                #[link_name=concat!("mw_com_gen_SamplePtr_", stringify!($uid), "_delete")]
                pub unsafe fn delete(sample_ptr: *mut $crate::proxy_bridge::SamplePtr<$ctype>);
                #[link_name=concat!("mw_com_gen_", stringify!($uid), "_get_size")]
                pub safe fn get_size() -> u32;
            }
        }

        impl $crate::proxy_bridge::EventOps for $ctype {
            fn get_sample_ref(ptr: &$crate::proxy_bridge::SamplePtr<Self>) -> &Self {
                unsafe { &*$uid::get(ptr) }
            }

            fn delete_sample_ptr(mut ptr: $crate::proxy_bridge::SamplePtr<Self>) {
                unsafe {
                    $uid::delete(&mut ptr);
                }
            }

            unsafe fn get_new_sample(
                proxy_event: *mut $crate::proxy_bridge::NativeProxyEvent<Self>,
            ) -> Option<$crate::proxy_bridge::SamplePtr<Self>> {
                let mut sample_ptr = std::mem::MaybeUninit::uninit();
                unsafe {
                    if $uid::get_new_sample(proxy_event, sample_ptr.as_mut_ptr()) {
                        Some(sample_ptr.assume_init())
                    } else {
                        None
                    }
                }
            }
        }

        impl $crate::TypeInfo for $ctype {
            fn get_size() -> u32 {
                $uid::get_size()
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
                pub fn get(proxy: *mut $crate::proxy_bridge::ProxyWrapperClass) -> *mut $crate::proxy_bridge::NativeProxyEvent<$ev_ty>;
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
                $crate::proxy_bridge::ProxyEvent::new(manager, stringify!($ev_name), |proxy| ffi::$ev_name::get(proxy.get_native_proxy()))
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
