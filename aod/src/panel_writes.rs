use std::sync::{atomic::{AtomicU64, Ordering}, Mutex};

// A newer display event invalidates old work, including delayed AOD callbacks.
pub struct PanelWrites {
    generation: AtomicU64,
    writer: Mutex<()>,
}

impl PanelWrites {
    pub const fn new() -> Self {
        Self { generation: AtomicU64::new(0), writer: Mutex::new(()) }
    }
    pub fn invalidate(&self) -> u64 {
        self.generation.fetch_add(1, Ordering::AcqRel).wrapping_add(1)
    }
    pub fn current(&self) -> u64 { self.generation.load(Ordering::Acquire) }
    pub fn apply(&self, ticket: u64, authorize: impl FnOnce() -> bool,
                 write: impl FnOnce()) -> bool {
        let _writer = self.writer.lock().unwrap_or_else(|error| error.into_inner());
        if self.current() != ticket || !authorize() || self.current() != ticket {
            return false;
        }
        write();
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Arc, Barrier};
    use std::thread;

    #[test]
    fn fod_request_cancels_delayed_aod_without_replacing_hint() {
        let gate = PanelWrites::new();
        let delayed_aod = gate.current();
        let fod = gate.invalidate();
        let panel = AtomicU64::new(0);
        assert!(gate.apply(fod, || true, || panel.store(100, Ordering::Release)));
        assert!(!gate.apply(delayed_aod, || true, || panel.store(28, Ordering::Release)));
        assert_eq!(panel.load(Ordering::Acquire), 100);
    }

    #[test]
    fn wake_during_power_query_cancels_the_inflight_dim_write() {
        let gate = Arc::new(PanelWrites::new());
        let queried = Arc::new(Barrier::new(2));
        let resume = Arc::new(Barrier::new(2));
        let panel = Arc::new(AtomicU64::new(900));
        let ticket = gate.invalidate();
        let worker = {
            let (gate, queried, resume, panel) =
                (gate.clone(), queried.clone(), resume.clone(), panel.clone());
            thread::spawn(move || gate.apply(ticket, || {
                queried.wait();
                resume.wait();
                true // The slow query returned an obsolete Dozing snapshot.
            }, || panel.store(28, Ordering::Release)))
        };
        queried.wait();
        gate.invalidate(); // Waking/ON event arrives while dumpsys is running.
        resume.wait();
        assert!(!worker.join().unwrap());
        assert_eq!(panel.load(Ordering::Acquire), 900);
    }

    #[test]
    fn current_doze_request_still_applies_but_unknown_power_does_not() {
        let gate = PanelWrites::new();
        let panel = AtomicU64::new(0);
        assert!(gate.apply(gate.invalidate(), || true,
                           || panel.store(28, Ordering::Release)));
        assert!(!gate.apply(gate.invalidate(), || false,
                            || panel.store(0, Ordering::Release)));
        assert_eq!(panel.load(Ordering::Acquire), 28);
    }
}
