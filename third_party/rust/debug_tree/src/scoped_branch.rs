use crate::TreeBuilder;

pub struct ScopedBranch {
    state: Option<TreeBuilder>,
}

impl ScopedBranch {
    pub fn new(state: TreeBuilder) -> ScopedBranch {
        state.enter();
        ScopedBranch { state: Some(state) }
    }
    pub fn none() -> ScopedBranch {
        ScopedBranch { state: None }
    }
    pub fn release(&mut self) {
        if let Some(x) = &self.state {
            x.exit();
        }
        self.state = None;
    }
}
impl Drop for ScopedBranch {
    fn drop(&mut self) {
        self.release();
    }
}
