use std::fs;
use std::io;
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
    fn fcntl(fd: i32, cmd: i32, ...) -> i32;
}

pub fn send_client_fd(stream: &UnixStream, fd: ServerFd, handle: u32) -> io::Result<()> {
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

    if unsafe { sendmsg(stream.as_raw_fd(), &msg, 0) } == -1 {
        Err(io::Error::last_os_error())
    } else {
        Ok(())
    }
}

pub fn recv_client_fd(stream: &UnixStream) -> io::Result<(u32, ClientFd, ServerFd)> {
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

    if unsafe { recvmsg(stream.as_raw_fd(), &msg, 0) } == -1 {
        Err(io::Error::last_os_error())
    } else {
        Ok((data[0].tid, data[0].fd, ctrl.fd))
    }
}

pub fn send_process_pipe(stream: &UnixStream, version: u32) -> io::Result<File> {
    let mut fds = [-1, -1];

    if unsafe { pipe(fds.as_mut_ptr()) } == -1 {
        Err(io::Error::last_os_error())?
    }
    send_client_fd(stream, fds[1], version)?;
    if unsafe { close(fds[1]) } == -1 {
        Err(io::Error::last_os_error())?
    }

    Ok(unsafe { File::from_raw_fd(fds[0]) })
}

fn create_server_lock() -> io::Result<fs::File> {
    let file = fs::File::create("lock")?;
    let meta = file.metadata()?;
    if !meta.is_file() {
        Err(io::Error::new(io::ErrorKind::Other, ""))
    } else {
        Ok(file)
    }
}

#[cfg(target_pointer_width = "32")]
#[repr(C, align(4))]
struct flock {
    l_type: i16,
    l_whence: i16,
    l_start: i32,
    l_len: i32,
    l_pid: i32,
}

#[cfg(target_pointer_width = "64")]
#[repr(C, align(8))]
struct flock {
    l_type: i16,
    l_whence: i16,
    l_start: i64,
    l_len: i64,
    l_pid: i32,
}

pub fn wait_for_lock() -> io::Result<()> {
    const F_SETLKW: i32 = 7;
    const F_RWLCK: i16 = 1;
    const SEEK_SET: i16 = 0;

    crate::create_server_dir()?;
    let lock = create_server_lock()?;
    let fl = flock {
        l_type: F_RWLCK,
        l_whence: SEEK_SET,
        l_start: 0,
        l_len: 1,
        l_pid: 0,
    };

    if unsafe { fcntl(lock.as_raw_fd(), F_SETLKW, &fl) } == -1 {
        Err(io::Error::last_os_error())
    } else {
        Ok(())
    }
}

/*
int kill_lock_owner( int sig )
{
    int fd, i, ret = 0;
    pid_t pid = 0;
    struct flock fl;

    server_dir = create_server_dir( 0 );
    if (!server_dir) return 0;  /* no server dir, nothing to do */

    fd = create_server_lock();

    for (i = 1; i <= 20; i++)
    {
        fl.l_type   = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start  = 0;
        fl.l_len    = 1;
        if (fcntl( fd, F_GETLK, &fl ) == -1) goto done;
        if (fl.l_type != F_WRLCK) goto done;  /* the file is not locked */
        if (!pid)  /* first time around */
        {
            if (!(pid = fl.l_pid)) goto done;  /* shouldn't happen */
            if (sig == -1)
            {
                if (kill( pid, SIGINT ) == -1) goto done;
                kill( pid, SIGCONT );
                ret = 1;
            }
            else  /* just send the specified signal and return */
            {
                ret = (kill( pid, sig ) != -1);
                goto done;
            }
        }
        else if (fl.l_pid != pid) goto done;  /* no longer the same process */
        usleep( 50000 * i );
    }
    /* waited long enough, now kill it */
    kill( pid, SIGKILL );

 done:
    close( fd );
    return ret;
}
*/
