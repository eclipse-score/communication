use std::path::Path;
use std::pin::pin;
use std::thread::sleep;
use std::time::Duration;

use futures::{Stream, StreamExt};

const SERVICE_DISCOVERY_SLEEP_DURATION: Duration = Duration::from_secs(1);
const DATA_RECEPTION_COUNT: usize = 100;

/// Async function that takes `count` samples from the stream and prints the `x` field of each
/// sample that is received.
async fn get_samples<
    'a,
    S: Stream<Item = mw_com::proxy::SamplePtr<'a, bigdata_gen_rs::MapApiLanesStamped>> + 'a,
>(
    map_api_lanes_stamped: S,
    count: usize,
) {
    let map_api_lanes_stamped = pin!(map_api_lanes_stamped);
    let mut limited_map_api_lanes_stamped = map_api_lanes_stamped.take(count);
    while let Some(data) = limited_map_api_lanes_stamped.next().await {
        println!("Received sample: {}", data.x);
    }
    println!("Stream ended");
}

/// Deliberately add Send to ensure that this is a future that can also be run by multithreaded
/// executors.
fn run<F: std::future::Future<Output = ()> + Send>(future: F) {
    futures::executor::block_on(future);
}

fn main() {
    println!(
        "[Rust] Size of MapApiLanesStamped: {}",
        std::mem::size_of::<bigdata_gen_rs::MapApiLanesStamped>()
    );
    println!(
        "[Rust] Size of MapApiLanesStamped::lane_boundaries: {}",
        std::mem::size_of_val(&bigdata_gen_rs::MapApiLanesStamped::default().lane_boundaries)
    );
    mw_com::initialize(Some(Path::new(
        "./score/mw/com/test/bigdata/mw_com_config.json",
    )));

    let instance_specifier = mw_com::InstanceSpecifier::try_from("score/cp60/MapApiLanesStamped")
        .expect("Instance specifier creation failed");

    let handles = loop {
        let handles = mw_com::proxy::find_service(instance_specifier.clone())
            .expect("Instance specifier resolution failed");
        if !handles.is_empty() {
            break handles;
        }
        println!("No service found, retrying in 1 second");
        sleep(SERVICE_DISCOVERY_SLEEP_DURATION);
    };

    let bigdata_gen_rs::BigData::Proxy {
        map_api_lanes_stamped_,
        ..
    } = bigdata_gen_rs::BigData::Proxy::new(&handles[0]).expect("Failed to create the proxy");
    let mut subscribed_map_api_lanes_stamped = map_api_lanes_stamped_
        .subscribe(1)
        .expect("Failed to subscribe");
    println!("Subscribed!");
    let map_api_lanes_stamped_stream = subscribed_map_api_lanes_stamped
        .as_stream()
        .expect("Failed to convert to stream");
    run(get_samples(
        map_api_lanes_stamped_stream,
        DATA_RECEPTION_COUNT,
    ));
}
