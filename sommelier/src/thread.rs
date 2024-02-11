use std::fs;
use std::mem;
use std::sync::{Arc, Mutex};
use std::thread;

use std::collections::HashMap;
use std::io::Read;
use std::os::fd::AsRawFd;
use std::os::unix::net::UnixStream;

const SERVER_PROTOCOL_VERSION: u32 = 793;

use crate::fd;

struct TidPool {
    next: u32,
    free: Vec<u32>,
}

impl TidPool {
    fn alloc(&mut self) -> Tid {
        match self.free.pop() {
            Some(tid) => Tid(tid),
            _ => {
                let tid = self.next;
                self.next += 4;
                Tid(tid)
            }
        }
    }

    fn free(&mut self, tid: &Tid) {
        let mut val = tid.0;

        while self.next - 4 == val {
            self.next -= 4;
            match self.free.pop() {
                Some(next) => val = next,
                _ => return,
            }
        }

        match self.free.binary_search(&val) {
            Err(pos) => self.free.insert(pos, val),
            _ => panic!(),
        }
    }
}

static TID_POOL: Mutex<TidPool> = Mutex::new(TidPool {
    next: 4,
    free: Vec::new(),
});

struct Tid(u32);

struct Thread {
    process: Arc<Mutex<Process>>,
    pipe: fs::File,
    tid: Tid,
}

impl Drop for Thread {
    fn drop(&mut self) {
        TID_POOL.lock().unwrap().free(&self.tid);
    }
}

impl Thread {
    fn run(&mut self) {
        let mut buffer = [0; 1024];
        let n = self.pipe.read(&mut buffer[..]).expect("error");
        println!("thread message: {:?}", &buffer[..n]);
        let (client, server) = self.recv_fd();
        println!(
            "thread id {} pipe {} client {} server {}",
            self.tid.0,
            self.pipe.as_raw_fd(),
            client,
            server
        );
    }

    fn recv_fd(&mut self) -> (fd::ClientFd, fd::ServerFd) {
        let mut process = self.process.lock().unwrap();
        let (client, server) = process.recv_fd(self.tid.0);
        (client, server)
    }
}

pub struct Process {
    stream: UnixStream,
    pid: Tid,

    fds: HashMap<u32, Vec<(fd::ClientFd, fd::ServerFd)>>,
}

impl Drop for Process {
    fn drop(&mut self) {
        TID_POOL.lock().unwrap().free(&self.pid);
    }
}

impl Process {
    pub fn spawn(stream: UnixStream) {
        thread::spawn(|| {
            let pipe = fd::send_process_pipe(&stream, SERVER_PROTOCOL_VERSION);

            let mut tids = TID_POOL.lock().unwrap();
            let (pid, tid) = (tids.alloc(), tids.alloc());
            mem::drop(tids);

            let process = Arc::new(Mutex::new(Process {
                stream,
                pid,

                fds: HashMap::new(),
            }));

            let mut thread = Thread {
                process,
                pipe,
                tid,
            };

            thread.run()
        });
    }

    fn recv_fd(&mut self, tid: u32) -> (fd::ClientFd, fd::ServerFd) {
        if !self.fds.contains_key(&tid) {
            self.fds.insert(tid, Vec::new());
        }

        loop {
            if let Some((client, server)) = self.fds.get_mut(&tid).unwrap().pop() {
                return (client, server);
            }

            let (dst, client, server) = fd::recv_client_fd(&self.stream);
            if dst == 0 {
                return (client, server);
            }

            self.fds.get_mut(&dst).unwrap().push((client, server));
        }
    }
}
