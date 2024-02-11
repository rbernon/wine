use std::mem;
use std::ptr;

use std::fs::File;
use std::os::fd::AsRawFd;
use std::os::fd::FromRawFd;
use std::os::unix::net::UnixStream;

pub type ServerFd = i32;
pub type ClientFd = i32;

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

#[cfg(target_pointer_width = "32")]
#[repr(C, align(4))]
struct cmsg {
    hdr: cmsghdr,
    fd: ServerFd,
}

#[cfg(target_pointer_width = "64")]
#[repr(C, align(8))]
struct cmsg {
    hdr: cmsghdr,
    fd: ServerFd,
}

extern "C" {
    fn pipe(fds: *mut i32) -> i32;
    fn close(fd: i32) -> i32;
    fn sendmsg(fd: i32, msg: *const msghdr, flags: i32) -> isize;
    fn recvmsg(fd: i32, msg: *const msghdr, flags: i32) -> isize;
}

pub fn send_client_fd(stream: &UnixStream, fd: ServerFd, handle: u32) {
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

    let socket = stream.as_raw_fd();
    assert!(unsafe { sendmsg(socket, &msg, 0) } != -1);
}

pub fn recv_client_fd(stream: &UnixStream) -> (u32, ClientFd, ServerFd) {
    #[derive(Default)]
    #[repr(C)]
    struct send_fd {
        tid: u32,
        fd: ClientFd,
    }

    let mut ctrl = cmsg {
        hdr: cmsghdr {
            cmsg_len: mem::size_of::<cmsg>(),
            cmsg_level: 0,
            cmsg_type: 0,
        },
        fd: -1,
    };

    let mut data: [send_fd; 1] = [send_fd::default()];
    let msg = msghdr {
        msg_name: ptr::null_mut(),
        msg_namelen: 0,
        msg_iov: &mut iovec {
            iov_base: data.as_mut_ptr() as *mut u8,
            iov_len: mem::size_of_val(&data),
        },
        msg_iovlen: 1,
        msg_control: &mut ctrl.hdr,
        msg_controllen: ctrl.hdr.cmsg_len,
        msg_flags: 0,
    };

    let socket = stream.as_raw_fd();
    assert!(unsafe { recvmsg(socket, &msg, 0) } != -1);

    (data[0].tid, data[0].fd, ctrl.fd)
}

pub fn send_process_pipe(stream: &UnixStream, version: u32) -> File {
    let mut fds = [-1, -1];

    assert!(unsafe { pipe(fds.as_mut_ptr()) } != -1);
    send_client_fd(stream, fds[1], version);
    assert!(unsafe { close(fds[1]) } != -1);

    return unsafe { File::from_raw_fd(fds[0]) };
}
