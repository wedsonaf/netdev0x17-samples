//! Rust echo server.
use kernel::net::{self, Ipv4Addr, SocketAddr, SocketAddrV4, TcpListener, TcpStream};
use kernel::task::{KTask, Task};
use kernel::{fmt, prelude::*};

fn echo_handler(_stream: TcpStream) {}

fn echo_listener(listener: TcpListener) {
    while !Task::should_stop() {
        let _ = listener
            .accept(true)
            .and_then(|s| Task::spawn(fmt!("handler"), || echo_handler(s)))
            .and_then(|t| Ok(t.detach()));
    }
}

struct EchoServer(KTask);

impl kernel::Module for EchoServer {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        let addr = SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::ANY, 8080));
        let listener = TcpListener::try_new(net::init_ns(), &addr)?;
        let task = Task::spawn(fmt!("listener"), || echo_listener(listener))?;
        Ok(Self(task))
    }
}

module! {
    type: EchoServer,
    name: "rust_echo_server",
    license: "GPL",
}
