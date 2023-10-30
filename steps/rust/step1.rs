//! Rust echo server.
use kernel::prelude::*;

struct EchoServer;
impl kernel::Module for EchoServer {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        Ok(Self)
    }
}

module! {
    type: EchoServer,
    name: "rust_echo_server",
    license: "GPL",
}
