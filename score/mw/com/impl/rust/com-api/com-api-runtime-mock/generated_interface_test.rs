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

use score_com::{
    Builder, CommData, FindServiceSpecifier, InstanceSpecifier, MockRuntimeBuilderImpl, OfferedProducer, Producer,
    ProviderInfo, Publisher, Reloc, Runtime, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};

#[derive(Clone, Debug, Reloc, CommData)]
#[repr(C)]
pub struct TestData {
    value: u32,
}

score_com::interface!(interface Generated { Id = "mock_test::Generated", event: Event<TestData> });

#[test]
fn generated_interface_roundtrip_through_public_mock_facade() {
    let runtime = MockRuntimeBuilderImpl::new().build().unwrap();
    let specifier = InstanceSpecifier::new("/mock_test/generated").unwrap();
    let offered = runtime
        .producer_builder::<GeneratedInterface>(specifier.clone())
        .build()
        .unwrap()
        .offer()
        .unwrap();
    let consumer = runtime
        .find_service::<GeneratedInterface>(FindServiceSpecifier::Specific(specifier))
        .get_available_instances()
        .unwrap()
        .pop()
        .unwrap()
        .build()
        .unwrap();
    let subscription = consumer.event.subscribe(2).unwrap();
    offered.event.send(TestData { value: 27 }).unwrap();
    let mut container = SampleContainer::new(2);
    assert_eq!(subscription.try_receive(&mut container, 2).unwrap(), 1);
    assert_eq!(container.front().unwrap().value, 27);
    drop(container);
    let _producer = offered.unoffer().unwrap();
}
