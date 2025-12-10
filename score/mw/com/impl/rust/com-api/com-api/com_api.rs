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

#[cfg(feature = "iceoryx")]
pub use com_api_runtime_iceoryx::IceoryxRuntimeImpl;
#[cfg(feature = "iceoryx")]
pub use com_api_runtime_iceoryx::RuntimeBuilderImpl as IceoryxRuntimeBuilderImpl;
#[cfg(not(feature = "iceoryx"))]
pub use com_api_runtime_lola::LolaRuntimeImpl;
#[cfg(not(feature = "iceoryx"))]
pub use com_api_runtime_lola::RuntimeBuilderImpl as LolaRuntimeBuilderImpl;
#[cfg(not(feature = "iceoryx"))]
pub use com_api_runtime_mock::MockRuntimeImpl;
#[cfg(not(feature = "iceoryx"))]
pub use com_api_runtime_mock::RuntimeBuilderImpl as MockRuntimeBuilderImpl;

pub use com_api_concept::{
    Builder, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, FindServiceSpecifier,
    InstanceSpecifier, Interface, OfferedProducer, Producer, ProducerBuilder, Publisher, Reloc,
    Result, Runtime, SampleContainer, SampleMaybeUninit, SampleMut, ServiceDiscovery, Subscriber,
    Subscription,
};
