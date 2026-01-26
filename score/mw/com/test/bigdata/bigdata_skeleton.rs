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
use std::path::Path;
use std::{thread, time};

fn main() {
    proxy_bridge_rs::initialize(Some(Path::new(
        "./score/mw/com/test/bigdata/mw_com_config.json",
    )));

    let instance_specifier =
        proxy_bridge_rs::InstanceSpecifier::try_from("score/cp60/MapApiLanesStamped")
            .expect("Instance specifier creation failed");

    let skeleton = bigdata_gen_rs::BigData::Skeleton::new(&instance_specifier)
        .expect("BigDataSkeleton creation failed");

    let skeleton = skeleton.offer_service().expect("Failed offering from rust");
    let mut x: u32 = 1;
    while x < 10 {
        let mut sample: bigdata_gen_rs::MapApiLanesStamped =
            bigdata_gen_rs::MapApiLanesStamped::default();
        sample.x = x;
        skeleton
            .events
            .map_api_lanes_stamped_
            .send(sample)
            .expect("Failed sending event");

        println!("published {x} sleeping");
        x += 1;
        thread::sleep(time::Duration::from_millis(100));
    }

    println!("stopping offering and sleeping for 5sec");
    let skeleton = skeleton.stop_offer_service();
    thread::sleep(time::Duration::from_secs(5));

    let skeleton = skeleton.offer_service().expect("Reoffering failed");
    x = 0;
    while x < 10 {
        let mut sample: bigdata_gen_rs::MapApiLanesStamped =
            bigdata_gen_rs::MapApiLanesStamped::default();
        sample.x = x;
        skeleton
            .events
            .map_api_lanes_stamped_
            .send(sample)
            .expect("Failed sending event");

        println!("published {x} sleeping");
        x += 1;
        thread::sleep(time::Duration::from_millis(100));
    }
}
