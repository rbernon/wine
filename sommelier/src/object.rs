use std::collections::HashMap;
use std::sync::Mutex;
use std::sync::Arc;

use crate::thread::*;

pub trait KernelObject : Send + Sync {
    fn grab(&self) -> Arc<dyn KernelObject>;

    fn make_permanent(&self) {
        let _ = Arc::into_raw(self.grab());
    }

    fn make_temporary(&self) {
        let ptr = Arc::into_raw(self.grab());
        unsafe {
            let _ = Arc::from_raw(ptr);
            Arc::decrement_strong_count(ptr)
        };
    }

    fn open(&self, process: &mut Process) -> Handle {
        process.handles.alloc(&self.grab())
    }
}

pub trait ObjectDirectory {
    fn lookup(&self, path: &String) -> Option<Arc<dyn KernelObject>>;
    fn insert(&mut self, path: &String, object: Arc<dyn KernelObject>);
}

pub struct Handle(pub u32);

pub struct HandleTable {
    next: u32,
    used: Vec<(u32, Arc<dyn KernelObject>)>,
}

impl HandleTable {
    pub fn new() -> HandleTable {
        HandleTable {
            next: 0,
            used: Vec::new(),
        }
    }

    pub fn alloc(&mut self, obj: &Arc<dyn KernelObject>) -> Handle {
        let (mut pos, idx) = match self.used.binary_search_by_key(&self.next, |&(a, _)| a) {
            Err(pos) => (pos, self.next),
            _ => panic!(),
        };

        self.used.insert(pos, (idx, obj.clone()));
        while let Some((next, _)) = self.used.get(pos) {
            if next != &self.next {
                break;
            }
            self.next += 1;
            pos += 1;
        }

        Handle((idx + 1) * 4)
    }

    pub fn free(&mut self, handle: &Handle) {
        let idx = handle.0 / 4 - 1;
        match self.used.binary_search_by_key(&idx, |&(a, _)| a) {
            Ok(pos) => self.used.remove(pos),
            _ => panic!(),
        };
    }

    pub fn get(&self, handle: &Handle) -> Option<Arc<dyn KernelObject>> {
        let idx = handle.0 / 4 - 1;
        match self.used.binary_search_by_key(&idx, |&(a, _)| a) {
            Ok(pos) => self.used.get(pos).map(|(_, b)| b.clone()),
            _ => None,
        }
    }
}

pub struct RootDirectory {
    objects: HashMap<String, Arc<dyn KernelObject>>,
}

impl RootDirectory {
    pub fn new() -> Arc<Mutex<RootDirectory>> {
        Arc::new(Mutex::new(RootDirectory {
            objects: HashMap::new(),
        }))
    }
}

impl ObjectDirectory for RootDirectory {
    fn lookup(&self, path: &String) -> Option<Arc<dyn KernelObject>> {
        self.objects.get(path).map(|a| a.clone())
    }

    fn insert(&mut self, path: &String, object: Arc<dyn KernelObject>) {
        self.objects.insert(path.to_string(), object);
    }
}
