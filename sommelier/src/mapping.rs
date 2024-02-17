use crate::object::*;
use crate::thread::*;
use std::fs;
use std::sync::Arc;
use std::sync::Weak;

struct Mapping {
    weak: Weak<Mapping>,
}

impl Mapping {
    pub fn new() -> Arc<Mapping> {
        Arc::new_cyclic(|weak| {
            Mapping { weak: weak.clone() }
        })
    }

    pub fn open(&self, process: &mut Process) -> Handle {
        process.handles.alloc(&self.grab())
    }
}

impl KernelObject for Mapping {
    fn grab(&self) -> Arc<dyn KernelObject> {
        self.weak.upgrade().unwrap()
    }
}
