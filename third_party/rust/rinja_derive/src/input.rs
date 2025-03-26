use std::borrow::Cow;
use std::collections::hash_map::{Entry, HashMap};
use std::fs::read_to_string;
use std::iter::FusedIterator;
use std::path::{Path, PathBuf};
use std::sync::{Arc, OnceLock};

use mime::Mime;
use parser::{Node, Parsed};
use proc_macro2::Span;
use rustc_hash::FxBuildHasher;
use syn::punctuated::Punctuated;
use syn::spanned::Spanned;

use crate::config::{Config, SyntaxAndCache};
use crate::{CompileError, FileInfo, MsgValidEscapers, OnceMap};

pub(crate) struct TemplateInput<'a> {
    pub(crate) ast: &'a syn::DeriveInput,
    pub(crate) config: &'a Config,
    pub(crate) syntax: &'a SyntaxAndCache<'a>,
    pub(crate) source: &'a Source,
    pub(crate) source_span: Option<Span>,
    pub(crate) block: Option<&'a str>,
    pub(crate) print: Print,
    pub(crate) escaper: &'a str,
    pub(crate) ext: Option<&'a str>,
    pub(crate) mime_type: String,
    pub(crate) path: Arc<Path>,
    pub(crate) fields: Vec<String>,
}

impl TemplateInput<'_> {
    /// Extract the template metadata from the `DeriveInput` structure. This
    /// mostly recovers the data for the `TemplateInput` fields from the
    /// `template()` attribute list fields.
    pub(crate) fn new<'n>(
        ast: &'n syn::DeriveInput,
        config: &'n Config,
        args: &'n TemplateArgs,
    ) -> Result<TemplateInput<'n>, CompileError> {
        let TemplateArgs {
            source,
            block,
            print,
            escaping,
            ext,
            ext_span,
            syntax,
            ..
        } = args;

        // Validate the `source` and `ext` value together, since they are
        // related. In case `source` was used instead of `path`, the value
        // of `ext` is merged into a synthetic `path` value here.
        let &(ref source, source_span) = source.as_ref().ok_or_else(|| {
            CompileError::new(
                #[cfg(not(feature = "code-in-doc"))]
                "specify one template argument `path` or `source`",
                #[cfg(feature = "code-in-doc")]
                "specify one template argument `path`, `source` or `in_doc`",
                None,
            )
        })?;
        let path = match (&source, &ext) {
            (Source::Path(path), _) => config.find_template(path, None, None)?,
            (&Source::Source(_), Some(ext)) => {
                PathBuf::from(format!("{}.{}", ast.ident, ext)).into()
            }
            (&Source::Source(_), None) => {
                return Err(CompileError::no_file_info(
                    #[cfg(not(feature = "code-in-doc"))]
                    "must include `ext` attribute when using `source` attribute",
                    #[cfg(feature = "code-in-doc")]
                    "must include `ext` attribute when using `source` or `in_doc` attribute",
                    None,
                ));
            }
        };

        // Validate syntax
        let syntax = syntax.as_deref().map_or_else(
            || Ok(config.syntaxes.get(config.default_syntax).unwrap()),
            |s| {
                config.syntaxes.get(s).ok_or_else(|| {
                    CompileError::no_file_info(format!("syntax `{s}` is undefined"), None)
                })
            },
        )?;

        // Match extension against defined output formats

        let escaping = escaping
            .as_deref()
            .or_else(|| path.extension().and_then(|s| s.to_str()))
            .unwrap_or_default();

        let escaper = config
            .escapers
            .iter()
            .find_map(|(extensions, path)| {
                extensions
                    .contains(&Cow::Borrowed(escaping))
                    .then_some(path.as_ref())
            })
            .ok_or_else(|| {
                CompileError::no_file_info(
                    format!(
                        "no escaper defined for extension '{escaping}'. {}",
                        MsgValidEscapers(&config.escapers),
                    ),
                    *ext_span,
                )
            })?;

        let mime_type =
            extension_to_mime_type(ext_default_to_path(ext.as_deref(), &path).unwrap_or("txt"))
                .to_string();

        let empty_punctuated = syn::punctuated::Punctuated::new();
        let fields = match ast.data {
            syn::Data::Struct(ref struct_) => {
                if let syn::Fields::Named(ref fields) = &struct_.fields {
                    &fields.named
                } else {
                    &empty_punctuated
                }
            }
            syn::Data::Union(ref union_) => &union_.fields.named,
            syn::Data::Enum(_) => &empty_punctuated,
        }
        .iter()
        .map(|f| match &f.ident {
            Some(ident) => ident.to_string(),
            None => unreachable!("we checked that we are using a struct"),
        })
        .collect::<Vec<_>>();

        Ok(TemplateInput {
            ast,
            config,
            syntax,
            source,
            source_span,
            block: block.as_deref(),
            print: *print,
            escaper,
            ext: ext.as_deref(),
            mime_type,
            path,
            fields,
        })
    }

    pub(crate) fn find_used_templates(
        &self,
        map: &mut HashMap<Arc<Path>, Arc<Parsed>, FxBuildHasher>,
    ) -> Result<(), CompileError> {
        let (source, source_path) = match &self.source {
            Source::Source(s) => (s.clone(), None),
            Source::Path(_) => (
                get_template_source(&self.path, None)?,
                Some(Arc::clone(&self.path)),
            ),
        };

        let mut dependency_graph = Vec::new();
        let mut check = vec![(Arc::clone(&self.path), source, source_path)];
        while let Some((path, source, source_path)) = check.pop() {
            let parsed = match self.syntax.parse(Arc::clone(&source), source_path) {
                Ok(parsed) => parsed,
                Err(err) => {
                    let msg = err
                        .message
                        .unwrap_or_else(|| "failed to parse template source".into());
                    let file_path = err
                        .file_path
                        .as_deref()
                        .unwrap_or(Path::new("<source attribute>"));
                    let file_info =
                        FileInfo::new(file_path, Some(&source), Some(&source[err.offset..]));
                    return Err(CompileError::new(msg, Some(file_info)));
                }
            };

            let mut top = true;
            let mut nested = vec![parsed.nodes()];
            while let Some(nodes) = nested.pop() {
                for n in nodes {
                    let mut add_to_check = |new_path: Arc<Path>| -> Result<(), CompileError> {
                        if let Entry::Vacant(e) = map.entry(new_path) {
                            // Add a dummy entry to `map` in order to prevent adding `path`
                            // multiple times to `check`.
                            let new_path = e.key();
                            let source = get_template_source(
                                new_path,
                                Some((&path, parsed.source(), n.span())),
                            )?;
                            check.push((new_path.clone(), source, Some(new_path.clone())));
                            e.insert(Arc::default());
                        }
                        Ok(())
                    };

                    match n {
                        Node::Extends(extends) if top => {
                            let extends = self.config.find_template(
                                extends.path,
                                Some(&path),
                                Some(FileInfo::of(extends, &path, &parsed)),
                            )?;
                            let dependency_path = (path.clone(), extends.clone());
                            if path == extends {
                                // We add the path into the graph to have a better looking error.
                                dependency_graph.push(dependency_path);
                                return cyclic_graph_error(&dependency_graph);
                            } else if dependency_graph.contains(&dependency_path) {
                                return cyclic_graph_error(&dependency_graph);
                            }
                            dependency_graph.push(dependency_path);
                            add_to_check(extends)?;
                        }
                        Node::Macro(m) if top => {
                            nested.push(&m.nodes);
                        }
                        Node::Import(import) if top => {
                            let import = self.config.find_template(
                                import.path,
                                Some(&path),
                                Some(FileInfo::of(import, &path, &parsed)),
                            )?;
                            add_to_check(import)?;
                        }
                        Node::FilterBlock(f) => {
                            nested.push(&f.nodes);
                        }
                        Node::Include(include) => {
                            let include = self.config.find_template(
                                include.path,
                                Some(&path),
                                Some(FileInfo::of(include, &path, &parsed)),
                            )?;
                            add_to_check(include)?;
                        }
                        Node::BlockDef(b) => {
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
                        Node::Lit(_)
                        | Node::Comment(_)
                        | Node::Expr(_, _)
                        | Node::Call(_)
                        | Node::Extends(_)
                        | Node::Let(_)
                        | Node::Import(_)
                        | Node::Macro(_)
                        | Node::Raw(_)
                        | Node::Continue(_)
                        | Node::Break(_) => {}
                    }
                }
                top = false;
            }
            map.insert(path, parsed);
        }
        Ok(())
    }

    #[inline]
    pub(crate) fn extension(&self) -> Option<&str> {
        ext_default_to_path(self.ext, &self.path)
    }
}

#[derive(Debug, Default)]
pub(crate) struct TemplateArgs {
    pub(crate) source: Option<(Source, Option<Span>)>,
    block: Option<String>,
    print: Print,
    escaping: Option<String>,
    ext: Option<String>,
    ext_span: Option<Span>,
    syntax: Option<String>,
    config: Option<String>,
    pub(crate) whitespace: Option<String>,
    pub(crate) template_span: Option<Span>,
    pub(crate) config_span: Option<Span>,
}

impl TemplateArgs {
    pub(crate) fn new(ast: &syn::DeriveInput) -> Result<Self, CompileError> {
        // Check that an attribute called `template()` exists at least once and that it is
        // the proper type (list).

        let mut templates_attrs = ast
            .attrs
            .iter()
            .filter(|attr| attr.path().is_ident("template"))
            .peekable();
        let mut args = match templates_attrs.peek() {
            Some(attr) => Self {
                template_span: Some(attr.path().span()),
                ..Self::default()
            },
            None => {
                return Err(CompileError::no_file_info(
                    "no attribute `template` found",
                    None,
                ));
            }
        };
        let attrs = templates_attrs
            .map(|attr| {
                type Attrs = Punctuated<syn::Meta, syn::Token![,]>;
                match attr.parse_args_with(Attrs::parse_terminated) {
                    Ok(args) => Ok(args),
                    Err(e) => Err(CompileError::no_file_info(
                        format!("unable to parse template arguments: {e}"),
                        Some(attr.path().span()),
                    )),
                }
            })
            .flat_map(ResultIter::from);

        // Loop over the meta attributes and find everything that we
        // understand. Return a CompileError if something is not right.
        // `source` contains an enum that can represent `path` or `source`.
        for item in attrs {
            let pair = match item? {
                syn::Meta::NameValue(pair) => pair,
                v => {
                    return Err(CompileError::no_file_info(
                        "unsupported attribute argument",
                        Some(v.span()),
                    ));
                }
            };

            let ident = match pair.path.get_ident() {
                Some(ident) => ident,
                None => unreachable!("not possible in syn::Meta::NameValue(…)"),
            };

            let mut value_expr = &pair.value;
            let value = loop {
                match value_expr {
                    syn::Expr::Lit(lit) => break lit,
                    syn::Expr::Group(group) => value_expr = &group.expr,
                    v => {
                        return Err(CompileError::no_file_info(
                            format!("unsupported argument value type for `{ident}`"),
                            Some(v.span()),
                        ));
                    }
                }
            };

            if ident == "path" {
                source_or_path(ident, value, &mut args.source, Source::Path)?;
                args.ext_span = Some(value.span());
            } else if ident == "source" {
                source_or_path(ident, value, &mut args.source, |s| Source::Source(s.into()))?;
            } else if ident == "in_doc" {
                source_from_docs(ident, value, &mut args.source, ast)?;
            } else if ident == "block" {
                set_template_str_attr(ident, value, &mut args.block)?;
            } else if ident == "print" {
                if let syn::Lit::Str(s) = &value.lit {
                    args.print = match s.value().as_str() {
                        "all" => Print::All,
                        "ast" => Print::Ast,
                        "code" => Print::Code,
                        "none" => Print::None,
                        v => {
                            return Err(CompileError::no_file_info(
                                format!("invalid value for `print` option: {v}"),
                                Some(s.span()),
                            ));
                        }
                    };
                } else {
                    return Err(CompileError::no_file_info(
                        "`print` value must be string literal",
                        Some(value.lit.span()),
                    ));
                }
            } else if ident == "escape" {
                set_template_str_attr(ident, value, &mut args.escaping)?;
            } else if ident == "ext" {
                set_template_str_attr(ident, value, &mut args.ext)?;
                args.ext_span = Some(value.span());
            } else if ident == "syntax" {
                set_template_str_attr(ident, value, &mut args.syntax)?;
            } else if ident == "config" {
                set_template_str_attr(ident, value, &mut args.config)?;
                args.config_span = Some(value.span());
            } else if ident == "whitespace" {
                set_template_str_attr(ident, value, &mut args.whitespace)?;
            } else {
                return Err(CompileError::no_file_info(
                    format!("unsupported attribute key `{ident}` found"),
                    Some(ident.span()),
                ));
            }
        }

        Ok(args)
    }

    pub(crate) fn fallback() -> Self {
        Self {
            source: Some((Source::Source("".into()), None)),
            ext: Some("txt".to_string()),
            ..Self::default()
        }
    }

    pub(crate) fn config_path(&self) -> Option<&str> {
        self.config.as_deref()
    }
}

/// Try to find the source in the comment, in a `rinja` code block.
///
/// This is only done if no path or source was given in the `#[template]` attribute.
fn source_from_docs(
    name: &syn::Ident,
    value: &syn::ExprLit,
    dest: &mut Option<(Source, Option<Span>)>,
    ast: &syn::DeriveInput,
) -> Result<(), CompileError> {
    match &value.lit {
        syn::Lit::Bool(syn::LitBool { value, .. }) => {
            if !value {
                return Ok(());
            }
        }
        lit => {
            return Err(CompileError::no_file_info(
                "argument `in_doc` expects as boolean value",
                Some(lit.span()),
            ));
        }
    };
    #[cfg(not(feature = "code-in-doc"))]
    {
        let _ = (name, dest, ast);
        Err(CompileError::no_file_info(
            "enable feature `code-in-doc` to use `in_doc` argument",
            Some(name.span()),
        ))
    }
    #[cfg(feature = "code-in-doc")]
    {
        ensure_source_once(name, dest)?;
        let (span, source) = collect_comment_blocks(name, ast)?;
        let source = strip_common_ws_prefix(source);
        let source = collect_rinja_code_blocks(name, ast, source)?;
        *dest = Some((source, span));
        Ok(())
    }
}

#[cfg(feature = "code-in-doc")]
fn collect_comment_blocks(
    name: &syn::Ident,
    ast: &syn::DeriveInput,
) -> Result<(Option<Span>, String), CompileError> {
    let mut span: Option<Span> = None;
    let mut assign_span = |kv: &syn::MetaNameValue| {
        // FIXME: uncomment once <https://github.com/rust-lang/rust/issues/54725> is stable
        // let new_span = kv.path.span();
        // span = Some(match span {
        //     Some(cur_span) => cur_span.join(new_span).unwrap_or(cur_span),
        //     None => new_span,
        // });

        if span.is_none() {
            span = Some(kv.path.span());
        }
    };

    let mut source = String::new();
    for a in &ast.attrs {
        // is a comment?
        let syn::Meta::NameValue(kv) = &a.meta else {
            continue;
        };
        if !kv.path.is_ident("doc") {
            continue;
        }

        // is an understood comment, e.g. not `#[doc = inline_str(…)]`
        let mut value = &kv.value;
        let value = loop {
            match value {
                syn::Expr::Lit(lit) => break lit,
                syn::Expr::Group(group) => value = &group.expr,
                _ => continue,
            }
        };
        let syn::Lit::Str(value) = &value.lit else {
            continue;
        };

        assign_span(kv);
        source.push_str(value.value().as_str());
        source.push('\n');
    }
    if source.is_empty() {
        return Err(no_rinja_code_block(name, ast));
    }

    Ok((span, source))
}

#[cfg(feature = "code-in-doc")]
fn no_rinja_code_block(name: &syn::Ident, ast: &syn::DeriveInput) -> CompileError {
    let kind = match &ast.data {
        syn::Data::Struct(_) => "struct",
        syn::Data::Enum(_) => "enum",
        syn::Data::Union(_) => "union",
    };
    CompileError::no_file_info(
        format!(
            "when using `in_doc = true`, the {kind}'s documentation needs a `rinja` code block"
        ),
        Some(name.span()),
    )
}

#[cfg(feature = "code-in-doc")]
fn strip_common_ws_prefix(source: String) -> String {
    let mut common_prefix_iter = source
        .lines()
        .filter_map(|s| Some(&s[..s.find(|c: char| !c.is_ascii_whitespace())?]));
    let mut common_prefix = common_prefix_iter.next().unwrap_or_default();
    for p in common_prefix_iter {
        if common_prefix.is_empty() {
            break;
        }
        let ((pos, _), _) = common_prefix
            .char_indices()
            .zip(p.char_indices())
            .take_while(|(l, r)| l == r)
            .last()
            .unwrap_or_default();
        common_prefix = &common_prefix[..pos];
    }
    if common_prefix.is_empty() {
        return source;
    }

    source
        .lines()
        .flat_map(|s| [s.get(common_prefix.len()..).unwrap_or_default(), "\n"])
        .collect()
}

#[cfg(feature = "code-in-doc")]
fn collect_rinja_code_blocks(
    name: &syn::Ident,
    ast: &syn::DeriveInput,
    source: String,
) -> Result<Source, CompileError> {
    use pulldown_cmark::{CodeBlockKind, Event, Parser, Tag, TagEnd};

    let mut tmpl_source = String::new();
    let mut in_rinja_code = false;
    let mut had_rinja_code = false;
    for e in Parser::new(&source) {
        match (in_rinja_code, e) {
            (false, Event::Start(Tag::CodeBlock(CodeBlockKind::Fenced(s)))) => {
                if s.split(",").any(|s| JINJA_EXTENSIONS.contains(&s)) {
                    in_rinja_code = true;
                    had_rinja_code = true;
                }
            }
            (true, Event::End(TagEnd::CodeBlock)) => in_rinja_code = false,
            (true, Event::Text(text)) => tmpl_source.push_str(&text),
            _ => {}
        }
    }
    if !had_rinja_code {
        return Err(no_rinja_code_block(name, ast));
    }

    if tmpl_source.ends_with('\n') {
        tmpl_source.pop();
    }
    Ok(Source::Source(tmpl_source.into()))
}

struct ResultIter<I, E>(Result<I, Option<E>>);

impl<I: IntoIterator, E> From<Result<I, E>> for ResultIter<I::IntoIter, E> {
    fn from(value: Result<I, E>) -> Self {
        Self(match value {
            Ok(i) => Ok(i.into_iter()),
            Err(e) => Err(Some(e)),
        })
    }
}

impl<I: Iterator, E> Iterator for ResultIter<I, E> {
    type Item = Result<I::Item, E>;

    fn next(&mut self) -> Option<Self::Item> {
        match &mut self.0 {
            Ok(iter) => Some(Ok(iter.next()?)),
            Err(err) => Some(Err(err.take()?)),
        }
    }
}

impl<I: FusedIterator, E> FusedIterator for ResultIter<I, E> {}

fn source_or_path(
    name: &syn::Ident,
    value: &syn::ExprLit,
    dest: &mut Option<(Source, Option<Span>)>,
    ctor: fn(String) -> Source,
) -> Result<(), CompileError> {
    ensure_source_once(name, dest)?;
    if let syn::Lit::Str(s) = &value.lit {
        *dest = Some((ctor(s.value()), Some(value.span())));
        Ok(())
    } else {
        Err(CompileError::no_file_info(
            format!("`{name}` value must be string literal"),
            Some(value.lit.span()),
        ))
    }
}

fn ensure_source_once(
    name: &syn::Ident,
    source: &mut Option<(Source, Option<Span>)>,
) -> Result<(), CompileError> {
    if source.is_none() {
        Ok(())
    } else {
        Err(CompileError::no_file_info(
            #[cfg(feature = "code-in-doc")]
            "must specify `source`, `path` or `is_doc` exactly once",
            #[cfg(not(feature = "code-in-doc"))]
            "must specify `source` or `path` exactly once",
            Some(name.span()),
        ))
    }
}

fn set_template_str_attr(
    name: &syn::Ident,
    value: &syn::ExprLit,
    dest: &mut Option<String>,
) -> Result<(), CompileError> {
    if dest.is_some() {
        Err(CompileError::no_file_info(
            format!("attribute `{name}` already set"),
            Some(name.span()),
        ))
    } else if let syn::Lit::Str(s) = &value.lit {
        *dest = Some(s.value());
        Ok(())
    } else {
        Err(CompileError::no_file_info(
            format!("`{name}` value must be string literal"),
            Some(value.lit.span()),
        ))
    }
}

#[inline]
fn ext_default_to_path<'a>(ext: Option<&'a str>, path: &'a Path) -> Option<&'a str> {
    ext.or_else(|| extension(path))
}

fn extension(path: &Path) -> Option<&str> {
    let ext = path.extension()?.to_str()?;
    if JINJA_EXTENSIONS.contains(&ext) {
        // an extension was found: file stem cannot be absent
        Path::new(path.file_stem().unwrap())
            .extension()
            .and_then(|s| s.to_str())
            .or(Some(ext))
    } else {
        Some(ext)
    }
}

#[derive(Debug, Hash, PartialEq)]
pub(crate) enum Source {
    Path(String),
    Source(Arc<str>),
}

#[derive(Clone, Copy, Debug, PartialEq, Hash)]
pub(crate) enum Print {
    All,
    Ast,
    Code,
    None,
}

impl Default for Print {
    fn default() -> Self {
        Self::None
    }
}

pub(crate) fn extension_to_mime_type(ext: &str) -> Mime {
    let basic_type = mime_guess::from_ext(ext).first_or_octet_stream();
    for (simple, utf_8) in &TEXT_TYPES {
        if &basic_type == simple {
            return utf_8.clone();
        }
    }
    basic_type
}

const TEXT_TYPES: [(Mime, Mime); 7] = [
    (mime::TEXT_PLAIN, mime::TEXT_PLAIN_UTF_8),
    (mime::TEXT_HTML, mime::TEXT_HTML_UTF_8),
    (mime::TEXT_CSS, mime::TEXT_CSS_UTF_8),
    (mime::TEXT_CSV, mime::TEXT_CSV_UTF_8),
    (
        mime::TEXT_TAB_SEPARATED_VALUES,
        mime::TEXT_TAB_SEPARATED_VALUES_UTF_8,
    ),
    (
        mime::APPLICATION_JAVASCRIPT,
        mime::APPLICATION_JAVASCRIPT_UTF_8,
    ),
    (mime::IMAGE_SVG, mime::IMAGE_SVG),
];

fn cyclic_graph_error(dependency_graph: &[(Arc<Path>, Arc<Path>)]) -> Result<(), CompileError> {
    Err(CompileError::no_file_info(
        format!(
            "cyclic dependency in graph {:#?}",
            dependency_graph
                .iter()
                .map(|e| format!("{:#?} --> {:#?}", e.0, e.1))
                .collect::<Vec<String>>()
        ),
        None,
    ))
}

pub(crate) fn get_template_source(
    tpl_path: &Arc<Path>,
    import_from: Option<(&Arc<Path>, &str, &str)>,
) -> Result<Arc<str>, CompileError> {
    static CACHE: OnceLock<OnceMap<Arc<Path>, Arc<str>>> = OnceLock::new();

    CACHE.get_or_init(OnceMap::default).get_or_try_insert(
        tpl_path,
        |tpl_path| match read_to_string(tpl_path) {
            Ok(mut source) => {
                if source.ends_with('\n') {
                    let _ = source.pop();
                }
                Ok((Arc::clone(tpl_path), Arc::from(source)))
            }
            Err(err) => Err(CompileError::new(
                format_args!(
                    "unable to open template file '{}': {err}",
                    tpl_path.to_str().unwrap(),
                ),
                import_from.map(|(node_file, file_source, node_source)| {
                    FileInfo::new(node_file, Some(file_source), Some(node_source))
                }),
            )),
        },
        Arc::clone,
    )
}

const JINJA_EXTENSIONS: &[&str] = &["j2", "jinja", "jinja2", "rinja"];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ext() {
        assert_eq!(extension(Path::new("foo-bar.txt")), Some("txt"));
        assert_eq!(extension(Path::new("foo-bar.html")), Some("html"));
        assert_eq!(extension(Path::new("foo-bar.unknown")), Some("unknown"));
        assert_eq!(extension(Path::new("foo-bar.svg")), Some("svg"));

        assert_eq!(extension(Path::new("foo/bar/baz.txt")), Some("txt"));
        assert_eq!(extension(Path::new("foo/bar/baz.html")), Some("html"));
        assert_eq!(extension(Path::new("foo/bar/baz.unknown")), Some("unknown"));
        assert_eq!(extension(Path::new("foo/bar/baz.svg")), Some("svg"));
    }

    #[test]
    fn test_double_ext() {
        assert_eq!(extension(Path::new("foo-bar.html.txt")), Some("txt"));
        assert_eq!(extension(Path::new("foo-bar.txt.html")), Some("html"));
        assert_eq!(extension(Path::new("foo-bar.txt.unknown")), Some("unknown"));

        assert_eq!(extension(Path::new("foo/bar/baz.html.txt")), Some("txt"));
        assert_eq!(extension(Path::new("foo/bar/baz.txt.html")), Some("html"));
        assert_eq!(
            extension(Path::new("foo/bar/baz.txt.unknown")),
            Some("unknown")
        );
    }

    #[test]
    fn test_skip_jinja_ext() {
        assert_eq!(extension(Path::new("foo-bar.html.j2")), Some("html"));
        assert_eq!(extension(Path::new("foo-bar.html.jinja")), Some("html"));
        assert_eq!(extension(Path::new("foo-bar.html.jinja2")), Some("html"));

        assert_eq!(extension(Path::new("foo/bar/baz.txt.j2")), Some("txt"));
        assert_eq!(extension(Path::new("foo/bar/baz.txt.jinja")), Some("txt"));
        assert_eq!(extension(Path::new("foo/bar/baz.txt.jinja2")), Some("txt"));
    }

    #[test]
    fn test_only_jinja_ext() {
        assert_eq!(extension(Path::new("foo-bar.j2")), Some("j2"));
        assert_eq!(extension(Path::new("foo-bar.jinja")), Some("jinja"));
        assert_eq!(extension(Path::new("foo-bar.jinja2")), Some("jinja2"));
    }

    #[test]
    fn get_source() {
        let path = Config::new("", None, None, None)
            .and_then(|config| config.find_template("b.html", None, None))
            .unwrap();
        assert_eq!(get_template_source(&path, None).unwrap(), "bar".into());
    }
}
