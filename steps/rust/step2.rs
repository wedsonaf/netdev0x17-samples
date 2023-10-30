//! Rust echo server.
use kernel::task::{KTask, Task};
use kernel::{fmt, prelude::*};

fn echo_listener() {}

struct EchoServer(KTask);

impl kernel::Module for EchoServer {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        let task = Task::spawn(fmt!("listener"), || echo_listener())?;
        Ok(Self(task))
    }
}

module! {
    type: EchoServer,
    name: "rust_echo_server",
    license: "GPL",
}
