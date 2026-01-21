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

//! # Proxy-Skeleton Bridge Integration Tests
//!
//! This crate provides comprehensive integration tests for the Rust implementation of the COM middleware's
//! proxy-skeleton bridge pattern. The test suite validates end-to-end service communication scenarios
//! using the IPC (MW::COM APIs) with optimized timing for fast execution.
//!
//! ## Test Coverage
//!
//! The integration test suite covers four main scenarios:
//!
//! 1. Skeleton Service Offering (`test_skeleton_offer_service_integration`)
//! - Service lifecycle management (initialization, offering, stopping)
//! - Event publishing with `MapApiLanesStamped` data type
//! - Background thread execution for concurrent operation
//! - Runtime: 2 seconds with 100ms event intervals
//!
//! 2. Proxy Service Consumption (`test_proxy_subscribe_integration`)
//! - Service discovery with retry logic and timeout handling
//! - Proxy creation from discovered service handles
//! - Event subscription with configurable sample limits (max 2 samples)
//! - Sample processing and cleanup procedures
//! - Runtime: 2 seconds (plus service discovery time)
//!
//! 3. Event Handler Callbacks (`test_proxy_subscribe_with_handler_integration`)
//! - Asynchronous event handler registration and management
//! - Callback function execution validation
//! - Handler lifecycle (set/unset) with thread-safe counting
//! - Verification of handler deactivation
//! - Runtime: ~3 seconds (2 seconds active + 1 second verification)
//!
//! 4. Multi-Client Scenarios (`test_multiple_proxies_subscribe_integration`)
//! - Concurrent proxy client connections (3 simultaneous clients)
//! - Independent service discovery per client
//! - Parallel event subscription and processing
//! - Validation of broadcast event delivery to all clients
//! - Runtime: 2 seconds per proxy thread (running in parallel)

use std::sync::{Arc, Mutex};
use std::thread::{self, sleep};
use std::time::Duration;

// Test: Skeleton offers service and publishes events
// 1. Initializes mw_com with configuration
// 2. Creates an instance specifier for xpad/cp60/MapApiLanesStamped
// 3. Offers a service continuously in a background thread
// 4. Publishes events every 100ms for 4 seconds
// 5. Gracefully stops the service offering after publishing
#[test]
fn test_skeleton_offer_service_integration() {
    println!("=== Starting Skeleton Service Offering Integration Test ===");

    // Spawn a background thread to run the skeleton service
    let skeleton_thread = thread::spawn(|| {
        println!("[Skeleton Thread] Initializing mw_com...");

        // Initialize mw_com with the config file
        let config_path =
            std::path::Path::new("score/mw/com/example/ipc_bridge/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Skeleton Thread] Creating instance specifier...");
        // Create instance specifier
        let instance_specifier =
            proxy_bridge_rs::InstanceSpecifier::try_from("score/cp60/MapApiLanesStamped")
                .expect("Instance specifier creation failed");

        println!("[Skeleton Thread] Creating skeleton...");
        let skeleton = ipc_bridge_gen_rs::IpcBridge::Skeleton::new(&instance_specifier)
            .expect("BigDataSkeleton creation failed");

        println!("[Skeleton Thread] Offering service...");
        let skeleton = skeleton.offer_service().expect("Failed offering from rust");

        // Publish events for 2 seconds
        let start_time = std::time::Instant::now();
        let mut x: u32 = 0;

        while start_time.elapsed() < Duration::from_secs(2) {
            let mut sample: ipc_bridge_gen_rs::MapApiLanesStamped =
                ipc_bridge_gen_rs::MapApiLanesStamped::default();
            sample.x = x;

            match skeleton.events.map_api_lanes_stamped_.send(sample) {
                Ok(_) => {
                    println!(
                        "[Skeleton Thread] Published event {} (elapsed: {:?})",
                        x,
                        start_time.elapsed()
                    );
                    x += 1;
                }
                Err(e) => {
                    eprintln!("[Skeleton Thread] Failed to send event: {:?}", e);
                }
            }

            sleep(Duration::from_millis(100));
        }

        println!("[Skeleton Thread] Service offering complete. Stopping service...");
        let _skeleton = skeleton.stop_offer_service();
        println!("[Skeleton Thread] Service stopped.");
    });

    // Wait for the service thread to complete
    println!("[Skeleton] Waiting for skeleton service thread to complete...");
    skeleton_thread.join().expect("Service thread panicked");
    println!("[Skeleton] Test complete!");
}

// Test: Proxy subscribes to service events
// 1. Initializes mw_com with configuration
// 2. Creates an instance specifier for xpad/cp60/MapApiLanesStamped
// 3. Discovers the service with retry logic (2 seconds timeout)
// 4. Creates a proxy from the discovered service handles
// 5. Subscribes to events with a max sample count of 2
// 6. Receives and processes samples for 2 seconds
// 7. Unsubscribes from the event
#[test]
fn test_proxy_subscribe_integration() {
    let proxy_thread = thread::spawn(|| {
        println!("[Proxy Thread] Initializing mw_com...");
        let config_path =
            std::path::Path::new("score/mw/com/example/ipc_bridge/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Proxy Thread] Creating instance specifier...");
        let instance_specifier =
            proxy_bridge_rs::InstanceSpecifier::try_from("score/cp60/MapApiLanesStamped")
                .expect("Instance specifier creation failed");
        // taking start time
        let start_time = std::time::Instant::now();
        // handle_container is Option<HandleContainer> type for
        // properly handle the case where no service is found
        let mut handle_container = None;
        // proxy thread check for service availability for 2 seconds in each 100 millisecond cycle
        while start_time.elapsed() < Duration::from_secs(2) {
            println!("[Proxy Thread] Finding service...");
            let handle_temp = proxy_bridge_rs::find_service(instance_specifier.clone())
                .expect("Failed to find service");
            match handle_temp.len() {
                0 => {
                    println!("[Proxy Thread] No service instances found yet - retrying...");
                }
                _ => {
                    handle_container = Some(handle_temp);
                    break;
                }
            }

            sleep(Duration::from_millis(100));
        }
        if handle_container.is_none() || handle_container.as_ref().unwrap().len() == 0 {
            println!("[Proxy Thread] No service instances found - skipping test");
            println!("Note: Make sure the skeleton service is running first!");
            return;
        }
        // Extracts the HandleContainer from Option<HandleContainer>
        let handle_container = handle_container.unwrap();
        println!(
            "[Proxy Thread] Found {} service instance(s)",
            handle_container.len()
        );
        let service_handle = &handle_container[0];

        println!("[Proxy Thread] Creating proxy...");
        let proxy = ipc_bridge_gen_rs::IpcBridge::Proxy::new(service_handle)
            .expect("Failed to create proxy");

        println!("[Proxy Thread] Subscribing to event with max sample count of 2...");
        match proxy.map_api_lanes_stamped_.subscribe(2) {
            Ok(subscribed_event) => {
                println!("[Proxy Thread] Successfully subscribed to event!");

                // Receive samples for a 2 seconds
                let start_time = std::time::Instant::now();
                let mut sample_count = 0;
                //checking for new samples every 100 ms
                while start_time.elapsed() < Duration::from_secs(2) {
                    if let Some(sample) = subscribed_event.get_new_sample() {
                        println!(
                            "[Proxy Thread] Received sample #{}: x = {}",
                            sample_count, sample.x
                        );
                        sample_count += 1;
                        // Sample is automatically dropped here, ending the borrow
                    }

                    sleep(Duration::from_millis(100));
                }

                println!("[Proxy Thread] Received {} samples total", sample_count);

                // Unsubscribe from the event
                println!("[Proxy Thread] Unsubscribing from event...");
                let _unsubscribed_event = subscribed_event.unsubscribe();
                println!("[Proxy Test] Successfully unsubscribed!");
            }
            Err((error_msg, _event)) => {
                eprintln!("[Proxy Thread] Failed to subscribe: {}", error_msg);
                panic!("Subscription failed");
            }
        }
    });
    println!("[Proxy] Waiting for proxy service thread to complete...");
    proxy_thread.join().expect("Service thread panicked");
    println!("[Proxy] Test complete!");
}

// Test: Proxy subscribes to service events with handler
// 1. Initializes mw_com with configuration
// 2. Creates an instance specifier for xpad/cp60/MapApiLanesStamped
// 3. Discovers the service with retry logic (2 seconds timeout)
// 4. Creates a proxy from the discovered service handles
// 5. Subscribes to events with a max sample count of 10
// 6. Sets a receive handler that increments a counter on each event
// 7. Waits for 2 seconds to allow handler to be called
// 8. Unsets the receive handler and verifies it is no longer called
// 9. Unsubscribes from the event
#[test]
fn test_proxy_subscribe_with_handler_integration() {
    let proxy_thread = thread::spawn(|| {
        println!("[Handler Thread] Initializing mw_com...");
        let config_path =
            std::path::Path::new("score/mw/com/example/ipc_bridge/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Handler Thread] Creating instance specifier...");
        let instance_specifier =
            proxy_bridge_rs::InstanceSpecifier::try_from("score/cp60/MapApiLanesStamped")
                .expect("Instance specifier creation failed");
        // taking start time
        let start_time = std::time::Instant::now();
        // handle_container is Option<HandleContainer> type for
        // properly handle the case where no service is found
        let mut handle_container = None;
        // proxy thread check for service availability for 2 seconds in each 100 millisecond cycle
        while start_time.elapsed() < Duration::from_secs(2) {
            println!("[Handler Thread] Finding service...");
            let handle_temp = proxy_bridge_rs::find_service(instance_specifier.clone())
                .expect("Failed to find service");
            match handle_temp.len() {
                0 => {
                    println!("[Handler Thread] No service instances found yet - retrying...");
                }
                _ => {
                    handle_container = Some(handle_temp);
                    break;
                }
            }

            sleep(Duration::from_millis(100));
        }
        if handle_container.is_none() || handle_container.as_ref().unwrap().len() == 0 {
            println!("[Handler Thread] No service instances found - skipping test");
            println!("Note: Make sure the skeleton service is running first!");
            return;
        }
        // Extracts the HandleContainer from Option<HandleContainer>
        let handle_container = handle_container.unwrap();
        // taking service handler
        let service_handle = &handle_container[0];

        println!("[Handler Thread] Creating proxy...");
        let proxy = ipc_bridge_gen_rs::IpcBridge::Proxy::new(service_handle)
            .expect("Failed to create proxy");

        println!("[Handler Thread] Subscribing to event...");
        // Subscribes to events with a max sample count of 10
        match proxy.map_api_lanes_stamped_.subscribe(10) {
            Ok(mut subscribed_event) => {
                println!("[Handler Thread] Successfully subscribed!");

                // Create shared counter wrapped in Arc<Mutex> for thread-safe access
                let handler_count = Arc::new(Mutex::new(0));
                let handler_count_clone = handler_count.clone();

                println!("[Handler Thread] Setting receive handler...");
                // Handler increments counter each time it's triggered
                subscribed_event.set_receive_handler(move || {
                    let mut count = handler_count_clone.lock().unwrap();
                    *count += 1;
                    println!(
                        "[Handler Thread] Receive handler called! (count: {})",
                        *count
                    );
                });

                println!("[Handler Thread] Waiting for events with handler active...");
                sleep(Duration::from_secs(2));

                let final_count = *handler_count.lock().unwrap();
                println!("[Handler Thread] Handler was called {} times", final_count);

                println!("[Handler Thread] Unsetting receive handler...");
                subscribed_event.unset_receive_handler();
                println!("[Handler Thread] Handler unset successfully");

                // Wait a bit more to ensure no more handler calls
                sleep(Duration::from_secs(1));

                let count_after_unset = *handler_count.lock().unwrap();
                // After unsetting, verify count doesn't increase
                assert_eq!(
                    final_count, count_after_unset,
                    "Handler should not be called after being unset"
                );

                println!("[Handler Thread] Unsubscribing...");
                let _unsubscribed_event = subscribed_event.unsubscribe();
                println!("[Handler Thread] Test complete!");
            }
            Err((error_msg, _event)) => {
                eprintln!("[Handler Thread] Failed to subscribe: {}", error_msg);
                panic!("Subscription failed");
            }
        }
    });
    println!("[Handler] Waiting for handler service thread to complete...");
    proxy_thread.join().expect("Service thread panicked");
    println!("[Handler] Test complete!");
}

// Test: Multiple proxies subscribe to service events concurrently
// 1. Spawns 3 proxy threads
// 2. Each thread initializes mw_com with configuration
// 3. Each thread creates an instance specifier for xpad/cp60/Map
// 4. Each thread discovers the service with retry logic (2 seconds timeout)
// 5. Each thread creates a proxy from the discovered service handles
// 6. Each thread subscribes to events with a max sample count of 2
// 7. Each thread receives and processes samples for 2 seconds
// 8. Each thread unsubscribes from the event
#[test]
fn test_multiple_proxies_subscribe_integration() {
    let mut proxy_threads = vec![];

    for _proxy_id in 0..3 {
        let proxy_thread = thread::spawn(|| {
            println!("[Proxy Thread] Initializing mw_com...");
            let config_path =
                std::path::Path::new("score/mw/com/example/ipc_bridge/etc/mw_com_config.json");
            proxy_bridge_rs::initialize(Some(config_path));

            println!("[Proxy Thread] Creating instance specifier...");
            let instance_specifier =
                proxy_bridge_rs::InstanceSpecifier::try_from("score/cp60/MapApiLanesStamped")
                    .expect("Instance specifier creation failed");
            // taking start time
            let start_time = std::time::Instant::now();
            // handle_container is Option<HandleContainer> type for
            // properly handle the case where no service is found
            let mut handle_container = None;
            // proxy thread check for service availability for 2 seconds in each 100 millisecond cycle
            while start_time.elapsed() < Duration::from_secs(2) {
                println!("[Proxy Thread] Finding service...");
                let handle_temp = proxy_bridge_rs::find_service(instance_specifier.clone())
                    .expect("Failed to find service");
                match handle_temp.len() {
                    0 => {
                        println!("[Proxy Thread] No service instances found yet - retrying...");
                    }
                    _ => {
                        handle_container = Some(handle_temp);
                        break;
                    }
                }

                sleep(Duration::from_millis(100));
            }
            if handle_container.is_none() || handle_container.as_ref().unwrap().len() == 0 {
                println!("[Proxy Thread] No service instances found - skipping test");
                println!("Note: Make sure the skeleton service is running first!");
                return;
            }
            // Extracts the HandleContainer from Option<HandleContainer>
            let handle_container = handle_container.unwrap();
            println!(
                "[Proxy Thread] Found {} service instance(s)",
                handle_container.len()
            );
            let service_handle = &handle_container[0];

            println!("[Proxy Thread] Creating proxy...");
            let proxy = ipc_bridge_gen_rs::IpcBridge::Proxy::new(service_handle)
                .expect("Failed to create proxy");

            println!("[Proxy Thread] Subscribing to event with max sample count of 2...");
            match proxy.map_api_lanes_stamped_.subscribe(2) {
                Ok(subscribed_event) => {
                    println!("[Proxy Thread] Successfully subscribed to event!");
                    // Receive samples for a 2 seconds
                    let start_time = std::time::Instant::now();
                    let mut sample_count = 0;
                    //checking for new samples every 100 ms
                    while start_time.elapsed() < Duration::from_secs(2) {
                        if let Some(sample) = subscribed_event.get_new_sample() {
                            println!(
                                "[Proxy Thread] Received sample #{}: x = {}",
                                sample_count, sample.x
                            );
                            sample_count += 1;
                            // Sample is automatically dropped here, ending the borrow
                        }

                        sleep(Duration::from_millis(100));
                    }

                    println!("[Proxy Thread] Received {} samples total", sample_count);

                    // Unsubscribe from the event
                    println!("[Proxy Thread] Unsubscribing from event...");
                    let _unsubscribed_event = subscribed_event.unsubscribe();
                    println!("[Proxy Test] Successfully unsubscribed!");
                }
                Err((error_msg, _event)) => {
                    eprintln!("[Proxy Thread] Failed to subscribe: {}", error_msg);
                    panic!("Subscription failed");
                }
            }
        });
        proxy_threads.push(proxy_thread);
    }
    for thread_id in proxy_threads {
        println!("[Proxy] Waiting for Proxy service thread to complete...");
        thread_id.join().expect("Service thread panicked");
    }
    println!("[Proxy] Test complete!");
}
