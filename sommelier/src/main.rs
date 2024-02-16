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

const SOCKET_NAME: &str = "socket";

fn get_prefix_dir() -> path::PathBuf {
    match env::var("WINEPREFIX") {
        Ok(val) => path::PathBuf::from(&val),
        _ => match env::var("HOME") {
            Ok(val) => path::Path::new(&val).join(".wine"),
            _ => panic!("Missing WINEPREFIX or HOME environment variables"),
        },
    }
}

fn get_server_dir(config_dir: &path::PathBuf) -> io::Result<path::PathBuf> {
    let meta = fs::metadata(config_dir)?;
    let mut buf = path::PathBuf::from("/tmp/");
    buf.push(format!(".wine-{}", meta.st_uid()));
    buf.push(format!("server-{:x}-{:x}", meta.st_dev(), meta.st_ino()));
    Ok(buf)
}

fn create_server_dir() -> io::Result<()> {
    let config_dir = get_prefix_dir();
    if let Err(e) = env::set_current_dir(&config_dir) {
        panic!(
            "Cannot set working directory to {}, {e}",
            config_dir.display()
        );
    }

    let server_dir = get_server_dir(&config_dir)?;
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

    Ok(())
}

fn open_master_socket() -> io::Result<()> {
    let socket = UnixListener::bind(SOCKET_NAME)?;

    let mut perms = fs::metadata(SOCKET_NAME)?.permissions();
    perms.set_mode(0o600);
    fs::set_permissions(SOCKET_NAME, perms)?;

    for stream in socket.incoming() {
        match stream {
            Ok(stream) => thread::Process::spawn(stream),
            Err(err) => panic!("{err}"),
        };
    }

    Ok(())
}

fn main() -> io::Result<()> {
    create_server_dir()?;
    open_master_socket()?;

    Ok(())
}
