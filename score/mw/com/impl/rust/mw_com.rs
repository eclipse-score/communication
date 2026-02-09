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
pub use macros::*;
pub use proxy_bridge_rs::{initialize, InstanceSpecifier}; // TODO: Move to common
pub mod proxy {
    pub use proxy_bridge_rs::{
        find_service, FatPtr, HandleContainer, HandleType, NativeInstanceSpecifier, ProxyEventBase,
        ProxyEventStream, ProxyManager, ProxyWrapperClass, SamplePtr, SubscribedProxyEvent,
    };
}
pub mod skeleton {
    pub use skeleton_bridge_rs::{OfferState, Offered, SkeletonEvent, SkeletonOps, UnOffered};
}
pub mod ffi {
    pub use proxy_bridge_rs::NativeInstanceSpecifier;
    pub use skeleton_bridge_rs::SkeletonWrapperClass;
}
pub use common::{Error, Result};
