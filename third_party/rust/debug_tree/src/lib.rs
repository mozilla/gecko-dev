use std::sync::{Arc, Mutex};

#[macro_use]
pub mod default;
mod internal;
pub mod scoped_branch;

pub mod defer;
mod test;
pub mod tree_config;

pub use default::default_tree;
use once_cell::sync::Lazy;
use scoped_branch::ScopedBranch;
use std::collections::BTreeMap;
use std::fs::File;
use std::io::Write;

pub use crate::tree_config::*;

/// Reference wrapper for `TreeBuilderBase`
#[derive(Debug, Clone)]
pub struct TreeBuilder(Arc<Mutex<internal::TreeBuilderBase>>);

impl TreeBuilder {
    /// Returns a new `TreeBuilder` with an empty `Tree`.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// ```
    pub fn new() -> TreeBuilder {
        TreeBuilder {
            0: Arc::new(Mutex::new(internal::TreeBuilderBase::new())),
        }
    }

    /// Set the configuration override for displaying trees
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::{TreeBuilder, add_branch_to, add_leaf_to, TreeSymbols, TreeConfig};
    /// let tree = TreeBuilder::new();
    /// {
    ///     add_branch_to!(tree, "1");
    ///     {
    ///         add_branch_to!(tree, "1.1");
    ///         add_leaf_to!(tree, "1.1.1");
    ///         add_leaf_to!(tree, "1.1.2");
    ///     }
    ///     add_leaf_to!(tree, "1.2");
    /// }
    /// add_leaf_to!(tree, "2");
    /// tree.set_config_override(TreeConfig::new()
    ///     .show_first_level()
    ///     .symbols(TreeSymbols::with_rounded()));
    /// tree.peek_print();
    /// assert_eq!("\
    /// ├╼ 1
    /// │ ├╼ 1.1
    /// │ │ ├╼ 1.1.1
    /// │ │ ╰╼ 1.1.2
    /// │ ╰╼ 1.2
    /// ╰╼ 2" , &tree.string());
    /// ```
    pub fn set_config_override(&self, config: TreeConfig) {
        let mut lock = self.0.lock().unwrap();
        lock.set_config_override(Some(config))
    }

    /// Remove the configuration override
    /// The default configuration will be used instead
    pub fn remove_config_override(&self) {
        self.0.lock().unwrap().set_config_override(None);
    }

    /// Update the configuration override for displaying trees
    /// If an override doesn't yet exist, it is created.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::{TreeBuilder, add_branch_to, add_leaf_to, TreeSymbols};
    /// let tree = TreeBuilder::new();
    /// {
    ///     add_branch_to!(tree, "1");
    ///     {
    ///         add_branch_to!(tree, "1.1");
    ///         add_leaf_to!(tree, "1.1.1");
    ///         add_leaf_to!(tree, "1.1.2");
    ///     }
    ///     add_leaf_to!(tree, "1.2");
    /// }
    /// add_leaf_to!(tree, "2");
    /// tree.update_config_override(|x|{
    ///     x.indent = 3;
    ///     x.symbols = TreeSymbols::with_rounded();
    ///     x.show_first_level = true;
    /// });
    /// tree.peek_print();
    /// assert_eq!("\
    /// ├─╼ 1
    /// │  ├─╼ 1.1
    /// │  │  ├─╼ 1.1.1
    /// │  │  ╰─╼ 1.1.2
    /// │  ╰─╼ 1.2
    /// ╰─╼ 2" , &tree.string());
    /// ```
    pub fn update_config_override<F: Fn(&mut TreeConfig)>(&self, update: F) {
        let mut lock = self.0.lock().unwrap();
        match lock.config_override_mut() {
            Some(x) => update(x),
            None => {
                let mut x = TreeConfig::default();
                update(&mut x);
                lock.set_config_override(Some(x));
            }
        }
    }

    /// Returns the optional configuration override.
    pub fn get_config_override(&self) -> Option<TreeConfig> {
        let lock = self.0.lock().unwrap();
        lock.config_override().clone()
    }

    /// Returns whether a configuration override is set.
    pub fn has_config_override(&self) -> bool {
        let lock = self.0.lock().unwrap();
        lock.config_override().is_some()
    }

    /// Adds a new branch with text, `text` and returns a `ScopedBranch`.
    /// When the returned `ScopedBranch` goes out of scope, (likely the end of the current block),
    /// or if its `release()` method is called, the tree will step back out of the added branch.
    ///
    /// # Arguments
    /// * `text` - A string slice to use as the newly added branch's text.
    ///
    /// # Examples
    ///
    /// Exiting branch when end of scope is reached.
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// {
    ///     let _branch = tree.add_branch("Branch"); // _branch enters scope
    ///     // tree is now pointed inside new branch.
    ///     tree.add_leaf("Child of Branch");
    ///     // _branch leaves scope, tree moves up to parent branch.
    /// }
    /// tree.add_leaf("Sibling of Branch");
    /// assert_eq!("\
    /// Branch
    /// └╼ Child of Branch
    /// Sibling of Branch" , &tree.string());
    /// ```
    ///
    /// Using `release()` before out of scope.
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// {
    ///     let mut branch = tree.add_branch("Branch"); // branch enters scope
    ///     // tree is now pointed inside new branch.
    ///     tree.add_leaf("Child of Branch");
    ///     branch.release();
    ///     tree.add_leaf("Sibling of Branch");
    ///     // branch leaves scope, but no effect because its `release()` method has already been called
    /// }
    /// assert_eq!("\
    /// Branch
    /// └╼ Child of Branch
    /// Sibling of Branch", &tree.string());
    /// ```
    pub fn add_branch(&self, text: &str) -> ScopedBranch {
        self.add_leaf(text);
        ScopedBranch::new(self.clone())
    }

    /// Adds a new branch with text, `text` and returns a `ScopedBranch`.
    /// When the returned `ScopedBranch` goes out of scope, (likely the end of the current block),
    /// or if its `release()` method is called, the tree tree will step back out of the added branch.
    ///
    /// # Arguments
    /// * `text` - A string slice to use as the newly added branch's text.
    ///
    /// # Examples
    ///
    /// Stepping out of branch when end of scope is reached.
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// {
    ///     tree.add_leaf("Branch");
    ///     let _branch = tree.enter_scoped(); // _branch enters scope
    ///     // tree is now pointed inside new branch.
    ///     tree.add_leaf("Child of Branch");
    ///     // _branch leaves scope, tree moves up to parent branch.
    /// }
    /// tree.add_leaf("Sibling of Branch");
    /// assert_eq!("\
    /// Branch
    /// └╼ Child of Branch
    /// Sibling of Branch", &tree.string());
    /// ```
    ///
    /// Using `release()` before out of scope.
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// {
    ///     tree.add_leaf("Branch");
    ///     let mut branch = tree.enter_scoped(); // branch enters scope
    ///     // tree is now pointed inside new branch.
    ///     tree.add_leaf("Child of Branch");
    ///     branch.release();
    ///     tree.add_leaf("Sibling of Branch");
    ///     // branch leaves scope, but no effect because its `release()` method has already been called
    /// }
    /// assert_eq!("\
    /// Branch
    /// └╼ Child of Branch
    /// Sibling of Branch", &tree.string());
    /// ```
    pub fn enter_scoped(&self) -> ScopedBranch {
        if self.is_enabled() {
            ScopedBranch::new(self.clone())
        } else {
            ScopedBranch::none()
        }
    }

    /// Adds a leaf to current branch with the given text, `text`.
    ///
    /// # Arguments
    /// * `text` - A string slice to use as the newly added leaf's text.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("New leaf");
    /// ```
    pub fn add_leaf(&self, text: &str) {
        let mut x = self.0.lock().unwrap();
        if x.is_enabled() {
            x.add_leaf(&text);
        }
    }

    /// Steps into a new child branch.
    /// Stepping out of the branch requires calling `exit()`.
    ///
    /// # Example
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Branch");
    /// tree.enter();
    /// tree.add_leaf("Child of Branch");
    /// assert_eq!("\
    /// Branch
    /// └╼ Child of Branch", &tree.string());
    /// ```
    pub fn enter(&self) {
        let mut x = self.0.lock().unwrap();
        if x.is_enabled() {
            x.enter();
        }
    }

    /// Exits the current branch, to the parent branch.
    /// If no parent branch exists, no action is taken
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Branch");
    /// tree.enter();
    /// tree.add_leaf("Child of Branch");
    /// tree.exit();
    /// tree.add_leaf("Sibling of Branch");
    /// assert_eq!("\
    /// Branch
    /// └╼ Child of Branch
    /// Sibling of Branch", &tree.string());
    /// ```
    pub fn exit(&self) -> bool {
        let mut x = self.0.lock().unwrap();
        if x.is_enabled() {
            x.exit()
        } else {
            false
        }
    }

    /// Returns the depth of the current branch
    /// The initial depth when no branches have been adeed is 0.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// assert_eq!(0, tree.depth());
    /// let _b = tree.add_branch("Branch");
    /// assert_eq!(1, tree.depth());
    /// let _b = tree.add_branch("Child branch");
    /// assert_eq!(2, tree.depth());
    /// ```
    pub fn depth(&self) -> usize {
        self.0.lock().unwrap().depth()
    }

    /// Prints the tree without clearing.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Leaf");
    /// tree.peek_print();
    /// // Leaf
    /// tree.peek_print();
    /// // Leaf
    /// // Leaf 2
    /// ```
    pub fn peek_print(&self) {
        self.0.lock().unwrap().peek_print();
    }

    /// Prints the tree and then clears it.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Leaf");
    /// tree.print();
    /// // Leaf
    /// tree.add_leaf("Leaf 2");
    /// tree.print();
    /// // Leaf 2
    /// ```
    pub fn print(&self) {
        self.0.lock().unwrap().print();
    }

    /// Returns the tree as a string without clearing the tree.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Leaf");
    /// assert_eq!("Leaf", tree.peek_string());
    /// tree.add_leaf("Leaf 2");
    /// assert_eq!("Leaf\nLeaf 2", tree.peek_string());
    /// ```
    pub fn peek_string(&self) -> String {
        self.0.lock().unwrap().peek_string()
    }

    /// Returns the tree as a string and clears the tree.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Leaf");
    /// assert_eq!("Leaf", tree.string());
    /// tree.add_leaf("Leaf 2");
    /// assert_eq!("Leaf 2", tree.string());
    /// ```
    pub fn string(&self) -> String {
        self.0.lock().unwrap().string()
    }

    /// Writes the tree to file without clearing.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// use std::fs::{read_to_string, create_dir};
    /// use std::io::Read;
    /// let tree = TreeBuilder::new();
    /// create_dir("test_out").ok();
    /// tree.add_leaf("Leaf");
    /// assert_eq!(tree.peek_string(), "Leaf");
    /// tree.peek_write("test_out/peek_write.txt");
    /// assert_eq!(read_to_string("test_out/peek_write.txt").unwrap(), "Leaf");
    /// assert_eq!(tree.peek_string(), "Leaf");
    /// ```
    pub fn peek_write(&self, path: &str) -> std::io::Result<()> {
        let mut file = File::create(path)?;
        file.write_all(self.peek_string().as_bytes())
    }

    /// Writes the tree to file without clearing.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// use std::io::Read;
    /// use std::fs::{read_to_string, create_dir};
    /// let tree = TreeBuilder::new();
    /// create_dir("test_out").ok();
    /// tree.add_leaf("Leaf");
    /// assert_eq!(tree.peek_string(), "Leaf");
    /// tree.write("test_out/write.txt");
    /// assert_eq!(read_to_string("test_out/write.txt").unwrap(), "Leaf");
    /// assert_eq!(tree.peek_string(), "");
    /// ```
    pub fn write(&self, path: &str) -> std::io::Result<()> {
        let mut file = File::create(path)?;
        file.write_all(self.string().as_bytes())
    }

    /// Clears the tree.
    ///
    /// # Example
    ///
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let tree = TreeBuilder::new();
    /// tree.add_leaf("Leaf");
    /// assert_eq!("Leaf", tree.peek_string());
    /// tree.clear();
    /// assert_eq!("", tree.peek_string());
    /// ```
    pub fn clear(&self) {
        self.0.lock().unwrap().clear()
    }

    /// Sets the enabled state of the tree.
    ///
    /// If not enabled, the tree will not be modified by adding leaves or branches.
    /// Additionally, if called using the `add_`... macros, arguments will not be processed.
    /// This is particularly useful for suppressing output in production, with very little overhead.
    ///
    /// # Example
    /// ```
    /// #[macro_use]
    /// use debug_tree::{TreeBuilder, add_leaf_to};
    /// let mut tree = TreeBuilder::new();
    /// tree.add_leaf("Leaf 1");
    /// tree.set_enabled(false);
    /// add_leaf_to!(tree, "Leaf 2");
    /// tree.set_enabled(true);
    /// add_leaf_to!(tree, "Leaf 3");
    /// assert_eq!("Leaf 1\nLeaf 3", tree.peek_string());
    /// ```
    pub fn set_enabled(&self, enabled: bool) {
        self.0.lock().unwrap().set_enabled(enabled);
    }

    /// Returns the enabled state of the tree.
    ///
    /// # Example
    /// ```
    /// use debug_tree::TreeBuilder;
    /// let mut tree = TreeBuilder::new();
    /// assert_eq!(true, tree.is_enabled());
    /// tree.set_enabled(false);
    /// assert_eq!(false, tree.is_enabled());
    /// ```
    pub fn is_enabled(&self) -> bool {
        self.0.lock().unwrap().is_enabled()
    }
}

pub trait AsTree {
    fn as_tree(&self) -> TreeBuilder;
    fn is_tree_enabled(&self) -> bool {
        self.as_tree().is_enabled()
    }
}

impl AsTree for TreeBuilder {
    fn as_tree(&self) -> TreeBuilder {
        self.clone()
    }
}

pub(crate) fn get_or_add_tree<T: AsRef<str>>(name: T) -> TreeBuilder {
    let mut map = TREE_MAP.lock().unwrap();
    match map.get(name.as_ref()) {
        Some(x) => x.clone(),
        _ => {
            let val = TreeBuilder::new();
            map.insert(name.as_ref().to_string(), val.clone());
            val
        }
    }
}

pub(crate) fn get_tree<T: AsRef<str>>(name: T) -> Option<TreeBuilder> {
    TREE_MAP.lock().unwrap().get(name.as_ref()).cloned()
}

type TreeMap = BTreeMap<String, TreeBuilder>;

static TREE_MAP: Lazy<Arc<Mutex<TreeMap>>> =
    Lazy::new(|| -> Arc<Mutex<TreeMap>> { Arc::new(Mutex::new(TreeMap::new())) });

/// Sets the enabled state of the tree.
///
/// # Arguments
/// * `name` - The tree name
/// * `enabled` - The enabled state
///
pub fn set_enabled<T: AsRef<str>>(name: T, enabled: bool) {
    let mut map = TREE_MAP.lock().unwrap();
    match map.get_mut(name.as_ref()) {
        Some(x) => x.set_enabled(enabled),
        _ => {
            let tree = TreeBuilder::new();
            tree.set_enabled(enabled);
            map.insert(name.as_ref().to_string(), tree);
        }
    }
}

impl<T: AsRef<str>> AsTree for T {
    fn as_tree(&self) -> TreeBuilder {
        get_or_add_tree(self)
    }
    /// Check if the named tree is enabled and exists
    /// This does not create a new tree if non-existent
    ///
    /// # Arguments
    /// * `tree_name` - The tree name
    ///
    fn is_tree_enabled(&self) -> bool {
        get_tree(self).map(|x| x.is_enabled()).unwrap_or(false)
    }
}

/// Returns the tree
/// If there is no tree then one is created and then returned.
pub fn tree<T: AsTree>(tree: T) -> TreeBuilder {
    tree.as_tree()
}

/// Returns the tree named `name`
/// If there is no tree named `name` then one is created and then returned.
pub fn is_tree_enabled<T: AsTree>(tree: &T) -> bool {
    tree.is_tree_enabled()
}

/// Calls [clear](TreeBuilder::clear) for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn clear<T: AsRef<str>>(name: T) {
    name.as_tree().clear();
}

/// Returns [string](TreeBuilder::string) for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn string<T: AsRef<str>>(name: T) -> String {
    name.as_tree().string()
}

/// Returns [peek_string](TreeBuilder::peek_string) for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn peek_string<T: AsRef<str>>(name: T) -> String {
    name.as_tree().peek_string()
}

/// Calls [print](TreeBuilder::print) for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn print<T: AsRef<str>>(name: T) {
    name.as_tree().print();
}

/// Calls [peek_print](TreeBuilder::peek_print)  for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn peek_print<T: AsRef<str>>(name: T) {
    name.as_tree().peek_print();
}

/// Calls [write](TreeBuilder::write) for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn write<T: AsRef<str>, P: AsRef<str>>(name: T, path: P) -> std::io::Result<()> {
    name.as_tree().write(path.as_ref())
}

/// Calls [peek_print](TreeBuilder::peek_print)  for the tree named `name`
/// If there is no tree named `name` then one is created
pub fn peek_write<T: AsRef<str>, P: AsRef<str>>(name: T, path: P) -> std::io::Result<()> {
    name.as_tree().peek_write(path.as_ref())
}

/// Adds a leaf to given tree with the given text and formatting arguments
///
/// # Arguments
/// * `tree` - The tree that the leaf should be added to
/// * `text...` - Formatted text arguments, as per `format!(...)`.
///
/// # Example
///
/// ```
/// #[macro_use]
/// use debug_tree::{TreeBuilder, add_leaf_to};
/// fn main() {
///     let tree = TreeBuilder::new();
///     add_leaf_to!(tree, "A {} leaf", "new");
///     assert_eq!("A new leaf", &tree.peek_string());
/// }
/// ```
#[macro_export]
macro_rules! add_leaf_to {
    ($tree:expr, $($arg:tt)*) => (if $crate::is_tree_enabled(&$tree) {
        use $crate::AsTree;
        $tree.as_tree().add_leaf(&format!($($arg)*))
    });
}

/// Adds a leaf to given tree with the given `value` argument
///
/// # Arguments
/// * `tree` - The tree that the leaf should be added to
/// * `value` - An expression that implements the `Display` trait.
///
/// # Example
///
/// ```
/// #[macro_use]
/// use debug_tree::{TreeBuilder, add_leaf_value_to};
/// fn main() {
///     let tree = TreeBuilder::new();
///     let value = add_leaf_value_to!(tree, 5 * 4 * 3 * 2);
///     assert_eq!(120, value);
///     assert_eq!("120", &tree.peek_string());
/// }
/// ```
#[macro_export]
macro_rules! add_leaf_value_to {
    ($tree:expr, $value:expr) => {{
        let v = $value;
        if $crate::is_tree_enabled(&$tree) {
            use $crate::AsTree;
            $tree.as_tree().add_leaf(&format!("{}", &v));
        }
        v
    }};
}

/// Adds a scoped branch to given tree with the given text and formatting arguments
/// The branch will be exited at the end of the current block.
///
/// # Arguments
/// * `tree` - The tree that the leaf should be added to
/// * `text...` - Formatted text arguments, as per `format!(...)`.
///
/// # Example
///
/// ```
/// #[macro_use]
/// use debug_tree::{TreeBuilder, add_branch_to, add_leaf_to};
/// fn main() {
///     let tree = TreeBuilder::new();
///     {
///         add_branch_to!(tree, "New {}", "Branch"); // _branch enters scope
///         // tree is now pointed inside new branch.
///         add_leaf_to!(tree, "Child of {}", "Branch");
///         // Block ends, so tree exits the current branch.
///     }
///     add_leaf_to!(tree, "Sibling of {}", "Branch");
///     assert_eq!("\
/// New Branch
/// └╼ Child of Branch
/// Sibling of Branch" , &tree.string());
/// }
/// ```
#[macro_export]
macro_rules! add_branch_to {
    ($tree:expr) => {
        let _debug_tree_branch = if $crate::is_tree_enabled(&$tree) {
            use $crate::AsTree;
            $tree.as_tree().enter_scoped()
        } else {
            $crate::scoped_branch::ScopedBranch::none()
        };
    };
    ($tree:expr, $($arg:tt)*) => {
        let _debug_tree_branch = if $crate::is_tree_enabled(&$tree) {
            use $crate::AsTree;
            $tree.as_tree().add_branch(&format!($($arg)*))
        } else {
            $crate::scoped_branch::ScopedBranch::none()
        };
    };
}

/// Calls `function` with argument, `tree`, at the end of the current scope
/// The function will only be executed if the tree is enabled when this macro is called
#[macro_export]
macro_rules! defer {
    ($function:expr) => {
        let _debug_tree_defer = {
            use $crate::AsTree;
            if $crate::default::default_tree().is_enabled() {
                use $crate::AsTree;
                $crate::defer::DeferredFn::new($crate::default::default_tree(), $function)
            } else {
                $crate::defer::DeferredFn::none()
            }
        };
    };
    ($tree:expr, $function:expr) => {
        let _debug_tree_defer = {
            use $crate::AsTree;
            if $tree.as_tree().is_enabled() {
                $crate::defer::DeferredFn::new($tree.as_tree(), $function)
            } else {
                $crate::defer::DeferredFn::none()
            }
        };
    };
}

/// Calls [print](TreeBuilder::print) on `tree` at the end of the current scope.
/// The function will only be executed if the tree is enabled when this macro is called
#[macro_export]
macro_rules! defer_print {
    () => {
        $crate::defer!(|x| {
            x.print();
        })
    };
    ($tree:expr) => {
        $crate::defer!($tree, |x| {
            x.print();
        })
    };
}

/// Calls [peek_print](TreeBuilder::peek_print) on `tree` at the end of the current scope.
/// The function will only be executed if the tree is enabled when this macro is called
#[macro_export]
macro_rules! defer_peek_print {
    () => {
        $crate::defer!(|x| {
            x.peek_print();
        })
    };
    ($tree:expr) => {
        $crate::defer!($tree, |x| {
            x.peek_print();
        })
    };
}

/// Calls [write](TreeBuilder::write) on `tree` at the end of the current scope.
/// The function will only be executed if the tree is enabled when this macro is called
#[macro_export]
macro_rules! defer_write {
    ($tree:expr, $path:expr) => {
        $crate::defer!($tree, |x| {
            if let Err(err) = x.write($path) {
                eprintln!("error during `defer_write`: {}", err);
            }
        })
    };
    ($path:expr) => {
        $crate::defer!(|x| {
            if let Err(err) = x.write($path) {
                eprintln!("error during `defer_write`: {}", err);
            }
        })
    };
}

/// Calls [peek_write](TreeBuilder::peek_write) on `tree` at the end of the current scope.
/// The function will only be executed if the tree is enabled when this macro is called
#[macro_export]
macro_rules! defer_peek_write {
    ($tree:expr, $path:expr) => {
        $crate::defer!($tree, |x| {
            if let Err(err) = x.peek_write($path) {
                eprintln!("error during `defer_peek_write`: {}", err);
            }
        })
    };
    ($path:expr) => {
        $crate::defer!(|x| {
            if let Err(err) = x.peek_write($path) {
                eprintln!("error during `defer_peek_write`: {}", err);
            }
        })
    };
}
