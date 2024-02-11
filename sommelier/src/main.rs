use std::env;
use std::fs;
use std::mem;
use std::path;
use std::ptr;
use std::rc;
use std::thread;

use std::os::fd::AsRawFd;
use std::os::fd::FromRawFd;
use std::os::linux::fs::MetadataExt;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{UnixListener, UnixStream};

const SOCKET_NAME: &str = "socket";
const SERVER_PROTOCOL_VERSION: u32 = 1;

#[repr(C)]
struct iovec {
    iov_base: *mut u8,
    iov_len: usize,
}

#[repr(C)]
struct cmsghdr {
    cmsg_len: usize,
    cmsg_level: i32,
    cmsg_type: i32,
}

#[repr(C)]
struct msghdr {
    msg_name: *mut u8,
    msg_namelen: u32,
    msg_iov: *mut iovec,
    msg_iovlen: usize,
    msg_control: *mut cmsghdr,
    msg_controllen: usize,
    msg_flags: i32,
}

extern "C" {
    fn pipe(fds: *mut i32) -> i32;
    fn close(fd: i32) -> i32;
    fn sendmsg(fd: i32, msg: *const msghdr, flags: i32) -> isize;
}

fn send_client_fd(stream: UnixStream, fd: i32, handle: u32) {
    #[cfg(target_pointer_width = "32")]
    #[repr(C, align(4))]
    struct cmsg {
        hdr: cmsghdr,
        fd: i32,
    }

    #[cfg(target_pointer_width = "64")]
    #[repr(C, align(8))]
    struct cmsg {
        hdr: cmsghdr,
        fd: i32,
    }

    let mut ctrl = cmsg {
        hdr: cmsghdr {
            cmsg_len: mem::size_of::<cmsg>(),
            cmsg_level: 1, /*SOL_SOCKET*/
            cmsg_type: 1,  /*SCM_RIGHTS*/
        },
        fd: fd,
    };

    let msg = msghdr {
        msg_name: ptr::null_mut(),
        msg_namelen: 0,
        msg_iov: &mut iovec {
            iov_base: handle.to_le_bytes().as_mut_ptr(),
            iov_len: mem::size_of_val(&handle),
        },
        msg_iovlen: 1,
        msg_control: &mut ctrl.hdr,
        msg_controllen: ctrl.hdr.cmsg_len,
        msg_flags: 0,
    };

    assert!(unsafe { sendmsg(stream.as_raw_fd(), &msg, 0) } != -1);
}

fn send_process_pipe(stream: UnixStream) -> UnixStream {
    let mut fds = [-1, -1];

    assert!(unsafe { pipe(fds.as_mut_ptr()) } != -1);
    send_client_fd(stream, fds[1], SERVER_PROTOCOL_VERSION);
    assert!(unsafe { close(fds[1]) } != -1);

    return unsafe { UnixStream::from_raw_fd(fds[0]) };
}

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
