use std::any::Any;
use std::collections::HashMap;
use std::sync::Arc;
use std::sync::Mutex;

use crate::thread::*;

pub trait ObjectArc {
    fn as_any(self: Arc<Self>) -> Arc<dyn Any + Send + Sync + 'static>;
    fn from_object(object: Arc<dyn Object>) -> Arc<Self>
    where
        Self: Sized;
}

impl<T: Object + 'static> ObjectArc for T {
    fn as_any(self: Arc<Self>) -> Arc<dyn Any + Send + Sync + 'static> {
        self
    }

    fn from_object(object: Arc<dyn Object>) -> Arc<Self>
    where
        Self: Sized,
    {
        object.as_any().downcast::<Self>().unwrap()
    }
}

pub trait Object: ObjectArc + Send + Sync {
    fn grab(&self) -> Arc<dyn Object>;

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

    fn get_unix_fd(&self) -> i32 {
        -1
    }
}

pub trait ObjectDirectory {
    fn lookup(&self, path: &String) -> Option<Arc<dyn Object>>;

    fn lookup_str(&mut self, path: &str) -> Option<Arc<dyn Object>> {
        self.lookup(&path.to_string())
    }

    fn insert(&mut self, path: &String, object: Arc<dyn Object>);

    fn insert_str(&mut self, path: &str, object: Arc<dyn Object>) {
        self.insert(&path.to_string(), object);
    }
}

pub struct Handle(pub u32);

pub struct HandleTable {
    next: u32,
    used: Vec<(u32, Arc<dyn Object>)>,
}

impl HandleTable {
    pub fn new() -> HandleTable {
        HandleTable {
            next: 0,
            used: Vec::new(),
        }
    }

    pub fn alloc(&mut self, obj: &Arc<dyn Object>) -> Handle {
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

    pub fn get(&self, handle: &Handle) -> Option<Arc<dyn Object>> {
        let idx = handle.0 / 4 - 1;
        match self.used.binary_search_by_key(&idx, |&(a, _)| a) {
            Ok(pos) => self.used.get(pos).map(|(_, b)| b.clone()),
            _ => None,
        }
    }
}

pub struct RootDirectory {
    objects: HashMap<String, Arc<dyn Object>>,
}

impl RootDirectory {
    pub fn new() -> Arc<Mutex<RootDirectory>> {
        Arc::new(Mutex::new(RootDirectory {
            objects: HashMap::new(),
        }))
    }
}

impl ObjectDirectory for RootDirectory {
    fn lookup(&self, path: &String) -> Option<Arc<dyn Object>> {
        self.objects.get(path).map(|a| a.clone())
    }

    fn insert(&mut self, path: &String, object: Arc<dyn Object>) {
        self.objects.insert(path.to_string(), object);
    }
}
