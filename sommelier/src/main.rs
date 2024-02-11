use std::env;
use std::fs;
use std::path;
use std::sync::mpsc;
use std::thread;

use std::collections::HashMap;
use std::io::Read;
use std::os::linux::fs::MetadataExt;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{UnixListener, UnixStream};

const SOCKET_NAME: &str = "socket";
const SERVER_PROTOCOL_VERSION: u32 = 793;

mod fd;

fn get_prefix_dir() -> path::PathBuf {
    match env::var("WINEPREFIX") {
        Ok(val) => path::PathBuf::from(&val),
        _ => match env::var("HOME") {
            Ok(val) => path::Path::new(&val).join(".wine"),
            _ => panic!("Missing WINEPREFIX or HOME environment variables"),
        },
    }
}

fn get_server_dir(config_dir: &path::PathBuf) -> path::PathBuf {
    let meta = fs::metadata(config_dir).unwrap();
    let mut buf = path::PathBuf::from("/tmp/");
    buf.push(format!(".wine-{}", meta.st_uid()));
    buf.push(format!("server-{:x}-{:x}", meta.st_dev(), meta.st_ino()));
    buf
}

fn create_server_dir() {
    let config_dir = get_prefix_dir();
    if let Err(e) = env::set_current_dir(&config_dir) {
        panic!(
            "Cannot set working directory to {}, {e}",
            config_dir.display()
        );
    }

    let server_dir = get_server_dir(&config_dir);
    if let Err(e) = fs::create_dir_all(&server_dir) {
        panic!(
            "Cannot create server directory {}, {e}",
            server_dir.display()
        );
    }
    if let Err(e) = env::set_current_dir(&server_dir) {
        panic!(
            "Cannot set working directory to {}, {e}",
            server_dir.display()
        );
    }
}

struct Thread {
    stream: fs::File,
    fds_queue: mpsc::Receiver<(fd::ClientFd, fd::ServerFd)>,
    server_fds: HashMap<fd::ClientFd, fd::ServerFd>,
    client_fds: HashMap<fd::ServerFd, fd::ClientFd>,
}

impl Thread {
    fn spawn(stream: fs::File) -> mpsc::Sender<(fd::ClientFd, fd::ServerFd)> {
        let (tx, rx) = mpsc::channel::<(fd::ClientFd, fd::ServerFd)>();
        thread::spawn(|| {
            Thread {
                stream,
                fds_queue: rx,
                server_fds: HashMap::new(),
                client_fds: HashMap::new(),
            }
            .run()
        });
        tx
    }

    fn run(&mut self) {
        let mut buffer = [0; 1024];
        let n = self.stream.read(&mut buffer[..]).expect("error");
        println!("thread message: {:?}", &buffer[..n]);
    }
}

struct Process {
    stream: UnixStream,
    fd_queues: HashMap<u32, mpsc::Sender<(fd::ClientFd, fd::ServerFd)>>,
}

impl Process {
    fn spawn(stream: UnixStream) {
        thread::spawn(|| {
            Process {
                stream,
                fd_queues: HashMap::new(),
            }
            .run()
        });
    }

    fn run(&mut self) {
        println!("new client");

        let pipe = fd::send_process_pipe(&self.stream, SERVER_PROTOCOL_VERSION);
        let thread = Thread::spawn(pipe);

        let mut buffer = [0; 1024];
        let n = self.stream.read(&mut buffer[..]).expect("error");
        println!("process message: {:?}", &buffer[..n]);

        loop {
            let (tid, client, server) = fd::recv_client_fd(&self.stream);
            println!("tid {} client {} server {}", tid, client, server);
            if tid == 0 {
                thread.send((client, server)).unwrap();
            } else {
                self.fd_queues[&tid].send((client, server)).unwrap();
            }
        }

        /*
                if ((process = create_process( client, NULL, 0, NULL, NULL, NULL, 0, NULL )))
                {
                    create_thread( -1, process, NULL );
                    release_object( process );
                }
        */
    }
}

fn open_master_socket() {
    let socket = UnixListener::bind(SOCKET_NAME).unwrap();

    let mut perms = fs::metadata(SOCKET_NAME).unwrap().permissions();
    perms.set_mode(0o600);
    fs::set_permissions(SOCKET_NAME, perms).unwrap();

    for stream in socket.incoming() {
        match stream {
            Ok(stream) => Process::spawn(stream),
            Err(err) => panic!("{err}"),
        };
    }
}

fn main() {
    println!("Hello, world!");
    println!("{}", get_prefix_dir().display());
    create_server_dir();
    open_master_socket();
}
