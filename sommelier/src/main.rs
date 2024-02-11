use std::env;
use std::path;

fn get_prefix_dir() -> path::PathBuf {
    match env::var("WINEPREFIX") {
        Ok(val) => path::PathBuf::from(&val),
        _ => match env::var("HOME") {
            Ok(val) => path::Path::new(&val).join(".wine"),
            _ => panic!("Missing WINEPREFIX or HOME environment variables"),
        },
    }
}

fn create_server_dir() {}

fn main() {
    println!("Hello, world!");
    println!("{}", get_prefix_dir().display());
}
