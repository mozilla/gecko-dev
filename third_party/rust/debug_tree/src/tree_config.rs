use once_cell::sync::Lazy;
use std::sync::{Arc, Mutex};

#[derive(Debug, Clone)]
pub struct TreeSymbols {
    /// A vertical base of the tree (│)
    pub continued: &'static str,

    /// Symbol for joining the first branch in a group (├)
    pub join_first: &'static str,

    /// Symbol for joining the last branch in a group (└)
    pub join_last: &'static str,

    /// Symbol for joining a branch that is not first or last in its group (├)
    pub join_inner: &'static str,

    /// Symbol for joining a branch if it is the only one in its group (├)
    pub join_only: &'static str,

    /// A repeated branch token (─)
    pub branch: &'static str,

    /// End of a leaf (╼)
    pub leaf: &'static str,

    pub multiline_first: Option<&'static str>,
    pub multiline_continued: Option<&'static str>,
}

#[derive(Debug, Clone)]
pub struct TreeConfig {
    pub symbols: TreeSymbols,

    /// Aside from the first branch, `indent` is equal to the number of spaces a child branch is
    /// shifted from its parent.
    pub indent: usize,

    pub show_first_level: bool,
}
impl TreeSymbols {
    pub fn new() -> Self {
        Self {
            continued: "│",
            join_first: "├",
            join_inner: "├",
            join_last: "└",
            join_only: "└",
            branch: "─",
            leaf: "╼ ",
            multiline_first: None,
            multiline_continued: None,
        }
    }
    pub fn with_pipes() -> Self {
        Self {
            continued: "║",
            join_first: "╠",
            join_inner: "╠",
            join_last: "╚",
            join_only: "╚",
            branch: "═",
            leaf: "╼ ",
            multiline_first: None,
            multiline_continued: None,
        }
    }
    pub fn with_thick() -> Self {
        Self {
            continued: "┃",
            join_first: "┣",
            join_inner: "┣",
            join_last: "┗",
            join_only: "┗",
            branch: "━",
            leaf: "╼ ",
            multiline_first: None,
            multiline_continued: None,
        }
    }
    pub fn with_rounded() -> Self {
        Self {
            continued: "│",
            join_first: "├",
            join_inner: "├",
            join_last: "╰",
            join_only: "╰",
            branch: "─",
            leaf: "╼ ",
            multiline_first: None,
            multiline_continued: None,
        }
    }
    pub fn with_dashed() -> Self {
        Self {
            continued: "┊",
            join_first: "┊",
            join_inner: "┊",
            join_last: "'",
            join_only: "'",
            branch: "╌",
            leaf: "- ",
            multiline_first: None,
            multiline_continued: None,
        }
    }

    pub fn continued(mut self, sym: &'static str) -> Self {
        self.continued = sym;
        self
    }
    pub fn join_first(mut self, sym: &'static str) -> Self {
        self.join_first = sym;
        self
    }
    pub fn join_inner(mut self, sym: &'static str) -> Self {
        self.join_inner = sym;
        self
    }
    pub fn join_last(mut self, sym: &'static str) -> Self {
        self.join_last = sym;
        self
    }
    pub fn join_only(mut self, sym: &'static str) -> Self {
        self.join_only = sym;
        self
    }

    pub fn branch(mut self, sym: &'static str) -> Self {
        self.branch = sym;
        self
    }
    pub fn leaf(mut self, sym: &'static str) -> Self {
        self.leaf = sym;
        self
    }
    pub fn multiline_first(mut self, sym: &'static str) -> Self {
        self.multiline_first = Some(sym);
        self
    }
    pub fn multiline_continued(mut self, sym: &'static str) -> Self {
        self.multiline_continued = Some(sym);
        self
    }
}

impl TreeConfig {
    pub fn new() -> Self {
        Self {
            symbols: TreeSymbols::new(),
            indent: 2,
            show_first_level: false,
        }
    }
    pub fn with_symbols(symbols: TreeSymbols) -> Self {
        Self {
            symbols,
            indent: 2,
            show_first_level: false,
        }
    }
    pub fn indent(mut self, x: usize) -> Self {
        self.indent = x;
        self
    }
    pub fn show_first_level(mut self) -> Self {
        self.show_first_level = true;
        self
    }
    pub fn hide_first_level(mut self) -> Self {
        self.show_first_level = false;
        self
    }
    pub fn symbols(mut self, x: TreeSymbols) -> Self {
        self.symbols = x;
        self
    }
}

impl Default for TreeSymbols {
    fn default() -> Self {
        tree_config_symbols()
    }
}
impl Default for TreeConfig {
    fn default() -> Self {
        tree_config()
    }
}

static DEFAULT_CONFIG: Lazy<Arc<Mutex<TreeConfig>>> =
    Lazy::new(|| -> Arc<Mutex<TreeConfig>> { Arc::new(Mutex::new(TreeConfig::new())) });

/// Set the default tree config
pub fn set_tree_config(x: TreeConfig) {
    *DEFAULT_CONFIG.lock().unwrap() = x;
}

/// The default tree config
pub fn tree_config() -> TreeConfig {
    DEFAULT_CONFIG.lock().unwrap().clone()
}

/// Set the default tree symbols config
pub fn set_tree_config_symbols(x: TreeSymbols) {
    DEFAULT_CONFIG.lock().unwrap().symbols = x;
}

/// The default tree symbols config
pub fn tree_config_symbols() -> TreeSymbols {
    DEFAULT_CONFIG.lock().unwrap().symbols.clone()
}

/// The default tree symbols config
pub fn update_tree_config<F: FnMut(&mut TreeConfig)>(mut update: F) {
    let mut x = DEFAULT_CONFIG.lock().unwrap();
    update(&mut x);
}

/// The default tree symbols config
pub fn update_tree_config_symbols<F: FnMut(&mut TreeSymbols)>(mut update: F) {
    let mut x = DEFAULT_CONFIG.lock().unwrap();
    update(&mut x.symbols);
}
