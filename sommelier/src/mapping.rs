use crate::object::*;
use crate::thread::*;
use std::fs;
use std::sync::Arc;
use std::sync::Weak;

pub struct Mapping {
    weak: Weak<Mapping>,
}

impl Mapping {
    pub fn new() -> Arc<Mapping> {
        Arc::new_cyclic(|weak| Mapping { weak: weak.clone() })
    }
}

impl KernelObject for Mapping {
    fn grab(&self) -> Arc<dyn KernelObject> {
        self.weak.upgrade().unwrap()
    }
}
