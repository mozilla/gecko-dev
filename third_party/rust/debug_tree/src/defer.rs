use crate::TreeBuilder;

/// A deferred function called with an argument, `TreeBuilder`
pub struct DeferredFn<F: Fn(TreeBuilder) -> ()> {
    tree: Option<TreeBuilder>,
    action: Option<F>,
}

impl<F> DeferredFn<F>
where
    F: Fn(TreeBuilder) -> (),
{
    /// Create a new deferred function based on `tree`
    pub fn new(tree: TreeBuilder, action: F) -> Self {
        DeferredFn {
            tree: Some(tree),
            action: Some(action),
        }
    }
    /// Create an empty deferred function
    /// This does nothing when scope ends
    pub fn none() -> Self {
        DeferredFn {
            tree: None,
            action: None,
        }
    }

    /// Disables the deferred function
    /// This prevents the function from executing when the scope ends
    pub fn cancel(&mut self) {
        self.tree = None;
        self.action = None;
    }
}

impl<F> Drop for DeferredFn<F>
where
    F: Fn(TreeBuilder) -> (),
{
    fn drop(&mut self) {
        if let (Some(x), Some(action)) = (&self.tree, &self.action) {
            action(x.clone());
        }
    }
}
