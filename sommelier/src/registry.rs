use std::any::Any;
use std::collections::HashMap;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::Weak;

use crate::object::*;

pub struct Registry {
    objects: HashMap<String, Arc<dyn Object>>,
    weak: Weak<Mutex<Registry>>,
}

impl Registry {
    pub fn new() -> Arc<Mutex<Registry>> {
        Arc::new_cyclic(|weak| {
            Mutex::new(Registry {
                objects: HashMap::new(),
                weak: weak.clone(),
            })
        })
    }
}

impl ObjectDirectory for Mutex<Registry> {
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
            let object = Registry::new();
            locked.objects.insert(name.to_string(), object.clone());
            object
        }
    }
}

impl Object for Mutex<Registry> {
    fn grab(&self) -> Arc<dyn Object> {
        let locked = self.lock().unwrap();
        locked.weak.upgrade().unwrap()
    }
}
