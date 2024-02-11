#![feature(unix_socket_ancillary_data)]

use std::env;
use std::ffi;
use std::fs;
use std::io;
use std::path;
use std::rc;
use std::thread;

use std::os::fd::FromRawFd;
use std::os::linux::fs::MetadataExt;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{SocketAncillary, UnixListener, UnixStream};

const SOCKET_NAME: &str = "socket";
const SERVER_PROTOCOL_VERSION: u32 = 1;

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
    stream: UnixStream,
    process: rc::Weak<Process>,
}

struct Process {
    stream: UnixStream,
    threads: Vec<Thread>,
}

extern "C" {
    fn pipe(fds: *mut ffi::c_int) -> ffi::c_int;
    fn close(fd: ffi::c_int) -> ffi::c_int;
}

fn send_client_fd(stream: UnixStream, fd: ffi::c_int, handle: u32) {
    let data: [u8; 4] = handle.to_le_bytes();
    let data_buf = &[io::IoSlice::new(&data)];

    let mut ancillary_buf = [0; 128];
    let mut ancillary = SocketAncillary::new(&mut ancillary_buf);
    ancillary.add_fds(&[fd]);

    stream.send_vectored_with_ancillary(data_buf, &mut ancillary).unwrap();
}

fn send_process_pipe(stream: UnixStream) -> UnixStream {
    let mut fds: [ffi::c_int; 2] = [-1, -1];

    assert!(unsafe { pipe(&mut fds as *mut ffi::c_int) } != -1);
    send_client_fd(stream, fds[1], SERVER_PROTOCOL_VERSION);
    assert!(unsafe { close(fds[1]) } != -1);

    return unsafe { UnixStream::from_raw_fd(fds[0]) };
}

fn handle_client(stream: UnixStream) {
    println!("new client");

    send_process_pipe(stream);
    /*
            if ((process = create_process( client, NULL, 0, NULL, NULL, NULL, 0, NULL )))
            {
                create_thread( -1, process, NULL );
                release_object( process );
            }
    */
}

fn open_master_socket() {
    let socket = UnixListener::bind(SOCKET_NAME).unwrap();

    let mut perms = fs::metadata(SOCKET_NAME).unwrap().permissions();
    perms.set_mode(0o600);
    fs::set_permissions(SOCKET_NAME, perms).unwrap();

    for stream in socket.incoming() {
        match stream {
            Ok(stream) => thread::spawn(|| handle_client(stream)),
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
