//! Rust echo server.
use kernel::kasync::executor::{workqueue::Executor, AutoStopHandle};
use kernel::kasync::net::{TcpListener, TcpStream};
use kernel::net::{self, Ipv4Addr, SocketAddr, SocketAddrV4};
use kernel::{prelude::*, spawn_task, sync::Arc};

async fn echo_handler(stream: TcpStream) -> Result {
    let mut buf = [0u8; 512];
    loop {
        let n = stream.read(&mut buf).await?;
        if n == 0 {
            return Ok(());
        }

        let mut to_write = &buf[..n];
        while !to_write.is_empty() {
            let written = stream.write(to_write).await?;
            to_write = &to_write[written..];
        }
    }
}

async fn echo_listener(listener: TcpListener, executor: Arc<Executor>) {
    loop {
        let _ = listener
            .accept()
            .await
            .and_then(|s| spawn_task!(executor.as_arc_borrow(), echo_handler(s)));
    }
}

struct EchoServer(AutoStopHandle<Executor>);

impl kernel::Module for EchoServer {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        let addr = SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::ANY, 8083));
        let listener = TcpListener::try_new(net::init_ns(), &addr)?;
        let handle = Executor::try_new(kernel::workqueue::system())?;
        spawn_task!(
            handle.executor(),
            echo_listener(listener, handle.executor().into())
        )?;
        Ok(Self(handle))
    }
}

module! {
    type: EchoServer,
    name: "rust_echo_server_async",
    license: "GPL",
}
