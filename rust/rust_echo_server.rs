//! Rust echo server.
use kernel::net::{self, Ipv4Addr, SocketAddr, SocketAddrV4, TcpListener, TcpStream};
use kernel::task::{KTask, Task};
use kernel::{fmt, prelude::*};

fn echo_handler(stream: TcpStream) -> Result {
    let mut buf = [0u8; 512];
    loop {
        let n = stream.read(&mut buf, true)?;
        if n == 0 {
            return Ok(());
        }

        let mut to_write = &buf[..n];
        while !to_write.is_empty() {
            let written = stream.write(to_write, true)?;
            to_write = &to_write[written..];
        }
    }
}

fn echo_listener(listener: TcpListener, module: &'static ThisModule) {
    while !Task::should_stop() {
        let _ = listener
            .accept(true)
            .and_then(|s| {
                Task::spawn_with_module(module, fmt!("handler"), || {
                    let _ = echo_handler(s);
                })
            })
            .and_then(|t| Ok(t.detach()));
    }
}

struct EchoServer(KTask);

impl kernel::Module for EchoServer {
    fn init(module: &'static ThisModule) -> Result<Self> {
        let addr = SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::ANY, 8080));
        let listener = TcpListener::try_new(net::init_ns(), &addr)?;
        let task = Task::spawn(fmt!("listener"), move || echo_listener(listener, module))?;
        Ok(Self(task))
    }
}

module! {
    type: EchoServer,
    name: "rust_echo_server",
    license: "GPL",
}
