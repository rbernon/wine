use std::sync::Arc;

trait KernelObject {}

struct Handle(u32);

struct HandleTable {
    next: u32,
    used: Vec<(u32, Arc<dyn KernelObject>)>,
}

impl HandleTable {
    fn alloc(&mut self, obj: &Arc<dyn KernelObject>) -> Handle {
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

    fn free(&mut self, handle: &Handle) {
        let idx = handle.0 / 4 - 1;
        match self.used.binary_search_by_key(&idx, |&(a, _)| a) {
            Ok(pos) => self.used.remove(pos),
            _ => panic!(),
        };
    }

    fn get(&self, handle: &Handle) -> Option<Arc<dyn KernelObject>> {
        let idx = handle.0 / 4 - 1;
        match self.used.binary_search_by_key(&idx, |&(a, _)| a) {
            Ok(pos) => self.used.get(pos).map(|(_, b)| b.clone()),
            _ => None,
        }
    }
}
