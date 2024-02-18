use std::io;

use crate::object::*;
use std::sync::Arc;
use std::sync::Weak;

extern "C" {
    fn memfd_create(name: *const u8, flags: u32) -> i32;
    fn ftruncate(fd: i32, len: usize) -> i32;
    fn close(fd: i32) -> i32;
}

pub struct Mapping {
    fd: i32,
    weak: Weak<Mapping>,
}

impl Mapping {
    pub fn new(size: usize) -> io::Result<Arc<Mapping>> {
        let data: Vec<u8> = vec![0];
        let fd = unsafe { memfd_create(data.as_ptr() as *const u8, 0) };
        if fd == -1 {
            Err(io::Error::last_os_error())?
        }

        if unsafe { ftruncate(fd, size) } == -1 {
            Err(io::Error::last_os_error())?
        }

        Ok(Arc::new_cyclic(|weak| Mapping {
            fd,
            weak: weak.clone(),
        }))
    }
}

impl Object for Mapping {
    fn grab(&self) -> Arc<dyn Object> {
        self.weak.upgrade().unwrap()
    }

    fn get_unix_fd(&self) -> i32 {
        self.fd
    }
}

impl Drop for Mapping {
    fn drop(&mut self) {
        println!("{:?}", self.fd);
        unsafe { close(self.fd) };
    }
}
