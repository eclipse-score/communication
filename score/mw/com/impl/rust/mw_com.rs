pub use macros::*;
pub use proxy_bridge_rs::{initialize, InstanceSpecifier};
pub mod proxy {
    pub use proxy_bridge_rs::{find_service, SamplePtr};
}
