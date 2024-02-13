use std::any::Any;
use std::collections::HashMap;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::Weak;

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

pub trait ObjectDirectory: Object {
    fn lookup(&self, name: &str) -> Option<Arc<dyn Object>>;

    fn lookup_path(&self, path: &str) -> Option<Arc<dyn Object>>
    where
        Self: Sized + 'static,
    {
        let mut object = self.grab();
        for part in path.trim_start_matches('\\').split('\\') {
            let directory = Self::from_object(object);
            object = directory.lookup(part)?;
        }
        Some(object)
    }

    fn insert(&self, name: &str, object: Arc<dyn Object>) -> Arc<dyn Object>;
    fn insert_dir(&self, name: &str) -> Arc<dyn Object>;

    fn insert_path(&self, path: &str, item: Arc<dyn Object>) -> Arc<dyn Object>
    where
        Self: Sized + 'static,
    {
        let mut split = path.trim_start_matches('\\').split('\\');
        let mut part = split.next().unwrap();
        let mut object = self.grab();

        while let Some(next) = split.next() {
            let directory = Self::from_object(object);
            object = directory.insert_dir(part);
            part = next;
        }

        let directory = Self::from_object(object);
        directory.insert(part, item)
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

pub struct Directory {
    objects: HashMap<String, Arc<dyn Object>>,
    weak: Weak<Mutex<Directory>>,
}

impl Directory {
    pub fn new() -> Arc<Mutex<Directory>> {
        Arc::new_cyclic(|weak| {
            Mutex::new(Directory {
                objects: HashMap::new(),
                weak: weak.clone(),
            })
        })
    }
}

impl ObjectDirectory for Mutex<Directory> {
    fn lookup(&self, path: &str) -> Option<Arc<dyn Object>> {
        let locked = self.lock().unwrap();
        locked.objects.get(path).map(|a| a.clone())
    }

    fn insert(&self, path: &str, object: Arc<dyn Object>) -> Arc<dyn Object> {
        let mut locked = self.lock().unwrap();
        locked.objects.insert(path.to_string(), object.clone());
        object
    }

    fn insert_dir(&self, name: &str) -> Arc<dyn Object> {
        let mut locked = self.lock().unwrap();
        if let Some(object) = locked.objects.get(name) {
            object.clone()
        } else {
            let object = Directory::new();
            locked.objects.insert(name.to_string(), object.clone());
            object
        }
    }
}

impl Object for Mutex<Directory> {
    fn grab(&self) -> Arc<dyn Object> {
        let locked = self.lock().unwrap();
        locked.weak.upgrade().unwrap()
    }
}
