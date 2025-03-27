use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;

use parser::node::{BlockDef, Macro};
use parser::{Node, Parsed, WithSpan};
use rustc_hash::FxBuildHasher;

use crate::config::Config;
use crate::{CompileError, FileInfo};

pub(crate) struct Heritage<'a> {
    pub(crate) root: &'a Context<'a>,
    pub(crate) blocks: BlockAncestry<'a>,
}

impl Heritage<'_> {
    pub(crate) fn new<'n>(
        mut ctx: &'n Context<'n>,
        contexts: &'n HashMap<&'n Arc<Path>, Context<'n>, FxBuildHasher>,
    ) -> Heritage<'n> {
        let mut blocks: BlockAncestry<'n> = ctx
            .blocks
            .iter()
            .map(|(name, def)| (*name, vec![(ctx, *def)]))
            .collect();

        while let Some(path) = &ctx.extends {
            ctx = &contexts[path];
            for (name, def) in &ctx.blocks {
                blocks.entry(name).or_default().push((ctx, def));
            }
        }

        Heritage { root: ctx, blocks }
    }
}

type BlockAncestry<'a> = HashMap<&'a str, Vec<(&'a Context<'a>, &'a BlockDef<'a>)>, FxBuildHasher>;

#[derive(Clone)]
pub(crate) struct Context<'a> {
    pub(crate) nodes: &'a [Node<'a>],
    pub(crate) extends: Option<Arc<Path>>,
    pub(crate) blocks: HashMap<&'a str, &'a BlockDef<'a>, FxBuildHasher>,
    pub(crate) macros: HashMap<&'a str, &'a Macro<'a>, FxBuildHasher>,
    pub(crate) imports: HashMap<&'a str, Arc<Path>, FxBuildHasher>,
    pub(crate) path: Option<&'a Path>,
    pub(crate) parsed: &'a Parsed,
}

impl Context<'_> {
    pub(crate) fn empty(parsed: &Parsed) -> Context<'_> {
        Context {
            nodes: &[],
            extends: None,
            blocks: HashMap::default(),
            macros: HashMap::default(),
            imports: HashMap::default(),
            path: None,
            parsed,
        }
    }

    pub(crate) fn new<'n>(
        config: &Config,
        path: &'n Path,
        parsed: &'n Parsed,
    ) -> Result<Context<'n>, CompileError> {
        let mut extends = None;
        let mut blocks = HashMap::default();
        let mut macros = HashMap::default();
        let mut imports = HashMap::default();
        let mut nested = vec![parsed.nodes()];
        let mut top = true;

        while let Some(nodes) = nested.pop() {
            for n in nodes {
                match n {
                    Node::Extends(e) => {
                        ensure_top(top, e, path, parsed, "extends")?;
                        if extends.is_some() {
                            return Err(CompileError::new(
                                "multiple extend blocks found",
                                Some(FileInfo::of(e, path, parsed)),
                            ));
                        }
                        extends = Some(config.find_template(
                            e.path,
                            Some(path),
                            Some(FileInfo::of(e, path, parsed)),
                        )?);
                    }
                    Node::Macro(m) => {
                        ensure_top(top, m, path, parsed, "macro")?;
                        macros.insert(m.name, &**m);
                    }
                    Node::Import(import) => {
                        ensure_top(top, import, path, parsed, "import")?;
                        let path = config.find_template(
                            import.path,
                            Some(path),
                            Some(FileInfo::of(import, path, parsed)),
                        )?;
                        imports.insert(import.scope, path);
                    }
                    Node::BlockDef(b) => {
                        blocks.insert(b.name, &**b);
                        nested.push(&b.nodes);
                    }
                    Node::If(i) => {
                        for cond in &i.branches {
                            nested.push(&cond.nodes);
                        }
                    }
                    Node::Loop(l) => {
                        nested.push(&l.body);
                        nested.push(&l.else_nodes);
                    }
                    Node::Match(m) => {
                        for arm in &m.arms {
                            nested.push(&arm.nodes);
                        }
                    }
                    _ => {}
                }
            }
            top = false;
        }

        Ok(Context {
            nodes: parsed.nodes(),
            extends,
            blocks,
            macros,
            imports,
            parsed,
            path: Some(path),
        })
    }

    pub(crate) fn generate_error<T>(&self, msg: &str, node: &WithSpan<'_, T>) -> CompileError {
        CompileError::new(
            msg,
            self.path.map(|path| FileInfo::of(node, path, self.parsed)),
        )
    }
}

fn ensure_top<T>(
    top: bool,
    node: &WithSpan<'_, T>,
    path: &Path,
    parsed: &Parsed,
    kind: &str,
) -> Result<(), CompileError> {
    if top {
        Ok(())
    } else {
        Err(CompileError::new(
            format!("`{kind}` blocks are not allowed below top level"),
            Some(FileInfo::of(node, path, parsed)),
        ))
    }
}
