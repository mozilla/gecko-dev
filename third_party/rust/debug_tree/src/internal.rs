use crate::tree_config::{tree_config, TreeConfig};
use std::cmp::max;
use std::sync::{Arc, Mutex};

/// Tree that holds `text` for the current leaf and a list of `children` that are the branches.
#[derive(Debug)]
pub struct Tree {
    pub text: Option<String>,
    pub children: Vec<Tree>,
}

/// Position of the element relative to its siblings
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Position {
    Inside,
    First,
    Last,
    Only,
}

impl Tree {
    /// Create a new tree with some optional text.
    pub fn new(text: Option<&str>) -> Tree {
        Tree {
            text: text.map(|x| x.to_string()),
            children: Vec::new(),
        }
    }

    /// Navigate to the branch at the given `path` relative to this tree.
    /// If a valid branch is found by following the path, it is returned.
    pub fn at_mut(&mut self, path: &[usize]) -> Option<&mut Tree> {
        match path.first() {
            Some(&i) => match self.children.get_mut(i) {
                Some(x) => x.at_mut(&path[1..]),
                _ => None,
            },
            _ => Some(self),
        }
    }

    /// "Render" this tree as a list of `String`s.
    /// Each string represents a line in the tree.
    /// `does_continue` is a bool for each column indicating whether the tree continues.
    pub fn lines(
        &self,
        does_continue: &Vec<bool>,
        index: usize,
        pool_size: usize,
        config: &TreeConfig,
    ) -> Vec<String> {
        let does_continue = if config.show_first_level && does_continue.is_empty() {
            vec![true]
        } else {
            does_continue.clone()
        };
        let position = match index {
            _ if pool_size == 1 => Position::Only,
            _ if (index + 1) == pool_size => Position::Last,
            0 => Position::First,
            _ => Position::Inside,
        };
        let mut next_continue = does_continue.clone();
        next_continue.push(match position {
            Position::Inside | Position::First => true,
            Position::Last | Position::Only => false,
        });

        let mut txt = String::new();
        let pad: String;
        if does_continue.len() > 1 {
            for &i in &does_continue[2..] {
                txt.push_str(&format!(
                    "{}{:indent$}",
                    if i { config.symbols.continued } else { " " },
                    "",
                    indent = max(config.indent, 1) - 1
                ));
            }
            pad = txt.clone();
            let branch_size = max(config.indent, 2usize) - 2;
            let branch = match config.symbols.branch.len() {
                0 => "-".repeat(branch_size),
                1 => config.symbols.branch.repeat(branch_size),
                _n => config
                    .symbols
                    .branch
                    .repeat(branch_size)
                    .chars()
                    .take(branch_size)
                    .collect::<String>(),
            };

            let is_multiline = self
                .text
                .as_ref()
                .map(|x| x.contains("\n"))
                .unwrap_or(false);

            let first_leaf = match (is_multiline, config.symbols.multiline_first) {
                (true, Some(x)) => x,
                _ => config.symbols.leaf,
            };
            txt.push_str(&format!(
                "{}{}{}",
                match position {
                    Position::Only => config.symbols.join_only,
                    Position::First => config.symbols.join_first,
                    Position::Last => config.symbols.join_last,
                    Position::Inside => config.symbols.join_inner,
                },
                branch,
                first_leaf,
            ));

            let s = match &self.text {
                Some(x) => match is_multiline {
                    true => format!(
                        "{}",
                        x.replace(
                            "\n",
                            &format!(
                                "\n{}{}{}{}",
                                &pad,
                                match position {
                                    Position::Only | Position::Last =>
                                        " ".repeat(config.symbols.continued.chars().count()),
                                    _ => config.symbols.continued.to_string(),
                                },
                                " ".repeat(branch_size),
                                match &config.symbols.multiline_continued {
                                    Some(multi) => multi.to_string(),
                                    _ => " ".repeat(first_leaf.chars().count()),
                                }
                            ),
                        )
                    ),
                    false => x.clone(),
                },
                _ => String::new(),
            };
            txt.push_str(&s);
        } else {
            if let Some(x) = &self.text {
                txt.push_str(&x);
            }
        }
        let mut ret = vec![txt];
        for (index, x) in self.children.iter().enumerate() {
            for line in x.lines(&next_continue, index, self.children.len(), config) {
                ret.push(line);
            }
        }
        ret
    }
}

/// Holds the current state of the tree, including the path to the branch.
/// Multiple trees may point to the same data.
#[derive(Debug, Clone)]
pub(crate) struct TreeBuilderBase {
    data: Arc<Mutex<Tree>>,
    path: Vec<usize>,
    dive_count: usize,
    config: Option<TreeConfig>,
    is_enabled: bool,
}

impl TreeBuilderBase {
    /// Create a new state
    pub fn new() -> TreeBuilderBase {
        TreeBuilderBase {
            data: Arc::new(Mutex::new(Tree::new(None))),
            path: vec![],
            dive_count: 1,
            config: None,
            is_enabled: true,
        }
    }

    pub fn set_enabled(&mut self, enabled: bool) {
        self.is_enabled = enabled;
    }
    pub fn is_enabled(&self) -> bool {
        self.is_enabled
    }

    pub fn add_leaf(&mut self, text: &str) {
        let &dive_count = &self.dive_count;
        if dive_count > 0 {
            for i in 0..dive_count {
                let mut n = 0;
                if let Some(x) = self.data.lock().unwrap().at_mut(&self.path) {
                    x.children.push(Tree::new(if i == max(1, dive_count) - 1 {
                        Some(&text)
                    } else {
                        None
                    }));
                    n = x.children.len() - 1;
                }
                self.path.push(n);
            }
            self.dive_count = 0;
        } else {
            if let Some(x) = self
                .data
                .lock()
                .unwrap()
                .at_mut(&self.path[..max(1, self.path.len()) - 1])
            {
                x.children.push(Tree::new(Some(&text)));
                let n = match self.path.last() {
                    Some(&x) => x + 1,
                    _ => 0,
                };
                self.path.last_mut().map(|x| *x = n);
            }
        }
    }

    pub fn set_config_override(&mut self, config: Option<TreeConfig>) {
        self.config = config;
    }

    pub fn config_override(&self) -> &Option<TreeConfig> {
        &self.config
    }
    pub fn config_override_mut(&mut self) -> &mut Option<TreeConfig> {
        &mut self.config
    }

    pub fn enter(&mut self) {
        self.dive_count += 1;
    }

    /// Try stepping up to the parent tree branch.
    /// Returns false if already at the top branch.
    pub fn exit(&mut self) -> bool {
        if self.dive_count > 0 {
            self.dive_count -= 1;
            true
        } else {
            if self.path.len() > 1 {
                self.path.pop();
                true
            } else {
                false
            }
        }
    }

    pub fn depth(&self) -> usize {
        max(1, self.path.len() + self.dive_count) - 1
    }

    pub fn peek_print(&self) {
        println!("{}", self.peek_string());
    }

    pub fn print(&mut self) {
        self.peek_print();
        self.clear();
    }
    pub fn clear(&mut self) {
        *self = Self::new();
    }

    pub fn string(&mut self) -> String {
        let s = self.peek_string();
        self.clear();
        s
    }

    pub fn peek_string(&self) -> String {
        let config = self
            .config_override()
            .clone()
            .unwrap_or_else(|| tree_config().clone());
        (&self.data.lock().unwrap().lines(&vec![], 0, 1, &config)[1..]).join("\n")
    }
}
