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

use crate::Debug;
use core::marker::PhantomData;
use std::path::{Path, PathBuf};

use crate::{
    LolaConsumerInfo, LolaProviderInfo, Publisher, LolaConsumerDiscovery, LolaProducerBuilder,
    SubscribableImpl,
};
use com_api_concept::{
    Builder, CommData, FindServiceSpecifier, InstanceSpecifier, Interface, Result, Runtime,
};

use bridge_ffi_rs::FFIBridge;
use bridge_ffi_lola::LolaFFIBridge;

pub struct LolaRuntimeImpl<B: FFIBridge = LolaFFIBridge> {
    _marker: PhantomData<B>,
}

impl<B: FFIBridge> Runtime for LolaRuntimeImpl<B> {
    type ServiceDiscovery<I: Interface + Send> = LolaConsumerDiscovery<I, B>;
    type Subscriber<T: CommData + Debug> = SubscribableImpl<T, B>;
    type ProducerBuilder<I: Interface> = LolaProducerBuilder<I, B>;
    type Publisher<T: CommData + Debug> = Publisher<T, B>;
    type ProviderInfo = LolaProviderInfo<B>;
    type ConsumerInfo = LolaConsumerInfo<B>;

    fn find_service<I: Interface + Send>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        LolaConsumerDiscovery {
            instance_specifier: match instance_specifier {
                FindServiceSpecifier::Any => panic!(
                    "FindServiceSpecifier::Any is not supported in LolaRuntimeImpl,
                Please use FindServiceSpecifier::Specific with a valid instance specifier."
                ),
                FindServiceSpecifier::Specific(spec) => spec,
            },
            _interface: PhantomData,
            _bridge: PhantomData,
        }
    }

    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I> {
        LolaProducerBuilder::new(self, instance_specifier)
    }
}

pub struct RuntimeBuilderImpl<B: FFIBridge = LolaFFIBridge> {
    config_path: Option<PathBuf>,
    _marker: PhantomData<B>,
}

impl<B: FFIBridge> Builder<LolaRuntimeImpl<B>> for RuntimeBuilderImpl<B> {
    fn build(self) -> Result<LolaRuntimeImpl<B>> {
        mw_com::initialize(self.config_path.as_deref());
        Ok(LolaRuntimeImpl {
            _marker: PhantomData,
        })
    }
}

impl<B: FFIBridge> com_api_concept::RuntimeBuilder<LolaRuntimeImpl<B>> for RuntimeBuilderImpl<B> {
    fn load_config(&mut self, config: &Path) -> &mut Self {
        self.config_path = Some(config.to_path_buf());
        self
    }
}

impl<B: FFIBridge> Default for RuntimeBuilderImpl<B> {
    fn default() -> Self {
        Self::new()
    }
}

impl<B: FFIBridge> RuntimeBuilderImpl<B> {
    /// Creates a new instance of the default implementation of the com layer
    pub fn new() -> Self {
        Self {
            config_path: None,
            _marker: PhantomData,
        }
    }
}
