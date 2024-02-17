use std::env;
use std::fs;
use std::io;
use std::path;

use std::os::linux::fs::MetadataExt;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::UnixListener;

mod fd;
mod ipc;
mod thread;

struct Mapping {
    fd: io::File,
    size: usize,
}

