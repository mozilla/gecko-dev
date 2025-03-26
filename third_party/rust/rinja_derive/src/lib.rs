#![cfg_attr(docsrs, feature(doc_cfg, doc_auto_cfg))]
#![deny(elided_lifetimes_in_paths)]
#![deny(unreachable_pub)]

mod config;
mod generator;
mod heritage;
mod html;
mod input;
#[cfg(test)]
mod tests;

use std::borrow::{Borrow, Cow};
use std::collections::hash_map::{Entry, HashMap};
use std::fmt;
use std::hash::{BuildHasher, Hash};
use std::path::Path;
use std::sync::Mutex;

use config::{Config, read_config_file};
use generator::{Generator, MapChain};
use heritage::{Context, Heritage};
use input::{Print, TemplateArgs, TemplateInput};
use parser::{Parsed, WithSpan, strip_common};
#[cfg(not(feature = "__standalone"))]
use proc_macro::TokenStream as TokenStream12;
#[cfg(feature = "__standalone")]
use proc_macro2::TokenStream as TokenStream12;
use proc_macro2::{Span, TokenStream};
use quote::quote_spanned;
use rustc_hash::FxBuildHasher;

/// The `Template` derive macro and its `template()` attribute.
///
/// Rinja works by generating one or more trait implementations for any
/// `struct` type decorated with the `#[derive(Template)]` attribute. The
/// code generation process takes some options that can be specified through
/// the `template()` attribute.
///
/// ## Attributes
///
/// The following sub-attributes are currently recognized:
///
/// ### path
///
/// E.g. `path = "foo.html"`
///
/// Sets the path to the template file.
/// The path is interpreted as relative to the configured template directories
/// (by default, this is a `templates` directory next to your `Cargo.toml`).
/// The file name extension is used to infer an escape mode (see below). In
/// web framework integrations, the path's extension may also be used to
/// infer the content type of the resulting response.
/// Cannot be used together with `source`.
///
/// ### source
///
/// E.g. `source = "{{ foo }}"`
///
/// Directly sets the template source.
/// This can be useful for test cases or short templates. The generated path
/// is undefined, which generally makes it impossible to refer to this
/// template from other templates. If `source` is specified, `ext` must also
/// be specified (see below). Cannot be used together with `path`.
/// `ext` (e.g. `ext = "txt"`): lets you specify the content type as a file
/// extension. This is used to infer an escape mode (see below), and some
/// web framework integrations use it to determine the content type.
/// Cannot be used together with `path`.
///
/// ### `in_doc`
///
/// E.g. `in_doc = true`
///
/// As an alternative to supplying the code template code in an external file (as `path` argument),
/// or as a string (as `source` argument), you can also enable the `"code-in-doc"` feature.
/// With this feature, you can specify the template code directly in the documentation
/// of the template `struct`.
///
/// Instead of `path = "…"` or `source = "…"`, specify `in_doc = true` in the `#[template]`
/// attribute, and in the struct's documentation add a `rinja` code block:
///
/// ```rust,ignore
/// /// ```rinja
/// /// <div>{{ lines|linebreaksbr }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     lines: &'a str,
/// }
/// ```
///
/// ### print
///
/// E.g. `print = "code"`
///
/// Enable debugging by printing nothing (`none`), the parsed syntax tree (`ast`),
/// the generated code (`code`) or `all` for both.
/// The requested data will be printed to stdout at compile time.
///
/// ### escape
///
/// E.g. `escape = "none"`
///
/// Override the template's extension used for the purpose of determining the escaper for
/// this template. See the section on configuring custom escapers for more information.
///
/// ### syntax
///
/// E.g. `syntax = "foo"`
///
/// Set the syntax name for a parser defined in the configuration file.
/// The default syntax, `"default"`,  is the one provided by Rinja.
#[allow(clippy::useless_conversion)] // To be compatible with both `TokenStream`s
#[cfg_attr(
    not(feature = "__standalone"),
    proc_macro_derive(Template, attributes(template))
)]
#[must_use]
pub fn derive_template(input: TokenStream12) -> TokenStream12 {
    let ast = match syn::parse2(input.into()) {
        Ok(ast) => ast,
        Err(err) => {
            let msgs = err.into_iter().map(|err| err.to_string());
            return compile_error(msgs, Span::call_site()).into();
        }
    };
    match build_template(&ast) {
        Ok(source) => source.parse().unwrap(),
        Err(CompileError {
            msg,
            span,
            rendered: _rendered,
        }) => {
            let mut ts = compile_error(std::iter::once(msg), span.unwrap_or(ast.ident.span()));
            if let Ok(source) = build_skeleton(&ast) {
                let source: TokenStream = source.parse().unwrap();
                ts.extend(source);
            }
            ts.into()
        }
    }
}

fn compile_error(msgs: impl Iterator<Item = String>, span: Span) -> TokenStream {
    let crate_ = syn::Ident::new(CRATE, Span::call_site());
    quote_spanned! {
        span =>
        const _: () = {
            extern crate #crate_ as rinja;
            #(rinja::helpers::core::compile_error!(#msgs);)*
        };
    }
}

fn build_skeleton(ast: &syn::DeriveInput) -> Result<String, CompileError> {
    let template_args = TemplateArgs::fallback();
    let config = Config::new("", None, None, None)?;
    let input = TemplateInput::new(ast, config, &template_args)?;
    let mut contexts = HashMap::default();
    let parsed = parser::Parsed::default();
    contexts.insert(&input.path, Context::empty(&parsed));
    Generator::new(
        &input,
        &contexts,
        None,
        MapChain::default(),
        input.block.is_some(),
        0,
    )
    .build(&contexts[&input.path])
}

/// Takes a `syn::DeriveInput` and generates source code for it
///
/// Reads the metadata from the `template()` attribute to get the template
/// metadata, then fetches the source from the filesystem. The source is
/// parsed, and the parse tree is fed to the code generator. Will print
/// the parse tree and/or generated source according to the `print` key's
/// value as passed to the `template()` attribute.
pub(crate) fn build_template(ast: &syn::DeriveInput) -> Result<String, CompileError> {
    let template_args = TemplateArgs::new(ast)?;
    let mut result = build_template_inner(ast, &template_args);
    if let Err(err) = &mut result {
        if err.span.is_none() {
            err.span = template_args
                .source
                .as_ref()
                .and_then(|(_, span)| *span)
                .or(template_args.template_span);
        }
    }
    result
}

fn build_template_inner(
    ast: &syn::DeriveInput,
    template_args: &TemplateArgs,
) -> Result<String, CompileError> {
    let config_path = template_args.config_path();
    let s = read_config_file(config_path, template_args.config_span)?;
    let config = Config::new(
        &s,
        config_path,
        template_args.whitespace.as_deref(),
        template_args.config_span,
    )?;
    let input = TemplateInput::new(ast, config, template_args)?;

    let mut templates = HashMap::default();
    input.find_used_templates(&mut templates)?;

    let mut contexts = HashMap::default();
    for (path, parsed) in &templates {
        contexts.insert(path, Context::new(input.config, path, parsed)?);
    }

    let ctx = &contexts[&input.path];
    let heritage = if !ctx.blocks.is_empty() || ctx.extends.is_some() {
        let heritage = Heritage::new(ctx, &contexts);

        if let Some(block_name) = input.block {
            if !heritage.blocks.contains_key(&block_name) {
                return Err(CompileError::no_file_info(
                    format!("cannot find block {block_name}"),
                    None,
                ));
            }
        }

        Some(heritage)
    } else {
        None
    };

    if input.print == Print::Ast || input.print == Print::All {
        eprintln!("{:?}", templates[&input.path].nodes());
    }

    let code = Generator::new(
        &input,
        &contexts,
        heritage.as_ref(),
        MapChain::default(),
        input.block.is_some(),
        0,
    )
    .build(&contexts[&input.path])?;
    if input.print == Print::Code || input.print == Print::All {
        eprintln!("{code}");
    }
    Ok(code)
}

#[derive(Debug, Clone)]
struct CompileError {
    msg: String,
    span: Option<Span>,
    rendered: bool,
}

impl CompileError {
    fn new<S: fmt::Display>(msg: S, file_info: Option<FileInfo<'_>>) -> Self {
        Self::new_with_span(msg, file_info, None)
    }

    fn new_with_span<S: fmt::Display>(
        msg: S,
        file_info: Option<FileInfo<'_>>,
        span: Option<Span>,
    ) -> Self {
        let msg = match file_info {
            Some(file_info) => format!("{msg}{file_info}"),
            None => msg.to_string(),
        };
        Self {
            msg,
            span,
            rendered: false,
        }
    }

    fn no_file_info<S: fmt::Display>(msg: S, span: Option<Span>) -> Self {
        Self {
            msg: msg.to_string(),
            span,
            rendered: false,
        }
    }
}

impl std::error::Error for CompileError {}

impl fmt::Display for CompileError {
    #[inline]
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.write_str(&self.msg)
    }
}

#[derive(Debug, Clone, Copy)]
struct FileInfo<'a> {
    path: &'a Path,
    source: Option<&'a str>,
    node_source: Option<&'a str>,
}

impl<'a> FileInfo<'a> {
    fn new(path: &'a Path, source: Option<&'a str>, node_source: Option<&'a str>) -> Self {
        Self {
            path,
            source,
            node_source,
        }
    }

    fn of<T>(node: &WithSpan<'a, T>, path: &'a Path, parsed: &'a Parsed) -> Self {
        Self {
            path,
            source: Some(parsed.source()),
            node_source: Some(node.span()),
        }
    }
}

impl fmt::Display for FileInfo<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if let (Some(source), Some(node_source)) = (self.source, self.node_source) {
            let (error_info, file_path) = generate_error_info(source, node_source, self.path);
            write!(
                f,
                "\n  --> {file_path}:{row}:{column}\n{source_after}",
                row = error_info.row,
                column = error_info.column,
                source_after = &error_info.source_after,
            )
        } else {
            let file_path = match std::env::current_dir() {
                Ok(cwd) => strip_common(&cwd, self.path),
                Err(_) => self.path.display().to_string(),
            };
            write!(f, "\n --> {file_path}")
        }
    }
}

struct ErrorInfo {
    row: usize,
    column: usize,
    source_after: String,
}

fn generate_row_and_column(src: &str, input: &str) -> ErrorInfo {
    let offset = src.len() - input.len();
    let (source_before, source_after) = src.split_at(offset);

    let source_after = match source_after.char_indices().enumerate().take(41).last() {
        Some((80, (i, _))) => format!("{:?}...", &source_after[..i]),
        _ => format!("{source_after:?}"),
    };

    let (row, last_line) = source_before.lines().enumerate().last().unwrap_or_default();
    let column = last_line.chars().count();
    ErrorInfo {
        row: row + 1,
        column,
        source_after,
    }
}

/// Return the error related information and its display file path.
fn generate_error_info(src: &str, input: &str, file_path: &Path) -> (ErrorInfo, String) {
    let file_path = match std::env::current_dir() {
        Ok(cwd) => strip_common(&cwd, file_path),
        Err(_) => file_path.display().to_string(),
    };
    let error_info: ErrorInfo = generate_row_and_column(src, input);
    (error_info, file_path)
}

struct MsgValidEscapers<'a>(&'a [(Vec<Cow<'a, str>>, Cow<'a, str>)]);

impl fmt::Display for MsgValidEscapers<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut exts = self
            .0
            .iter()
            .flat_map(|(exts, _)| exts)
            .map(|x| format!("{x:?}"))
            .collect::<Vec<_>>();
        exts.sort();
        write!(f, "The available extensions are: {}", exts.join(", "))
    }
}

#[derive(Debug)]
struct OnceMap<K, V>([Mutex<HashMap<K, V, FxBuildHasher>>; 8]);

impl<K, V> Default for OnceMap<K, V> {
    fn default() -> Self {
        Self(Default::default())
    }
}

impl<K: Hash + Eq, V> OnceMap<K, V> {
    // The API of this function was copied, and adapted from the `once_map` crate
    // <https://crates.io/crates/once_map/0.4.18>.
    fn get_or_try_insert<T, Q, E>(
        &self,
        key: &Q,
        make_key_value: impl FnOnce(&Q) -> Result<(K, V), E>,
        to_value: impl FnOnce(&V) -> T,
    ) -> Result<T, E>
    where
        K: Borrow<Q>,
        Q: Hash + Eq,
    {
        let shard_idx = (FxBuildHasher.hash_one(key) % self.0.len() as u64) as usize;
        let mut shard = self.0[shard_idx].lock().unwrap();
        Ok(to_value(if let Some(v) = shard.get(key) {
            v
        } else {
            let (k, v) = make_key_value(key)?;
            match shard.entry(k) {
                Entry::Vacant(entry) => entry.insert(v),
                Entry::Occupied(_) => unreachable!("key in map when it should not have been"),
            }
        }))
    }
}

// This is used by the code generator to decide whether a named filter is part of
// Rinja or should refer to a local `filters` module.
const BUILT_IN_FILTERS: &[&str] = &[
    "capitalize",
    "center",
    "indent",
    "lower",
    "lowercase",
    "title",
    "trim",
    "truncate",
    "upper",
    "urlencode",
    "wordcount",
];

const CRATE: &str = if cfg!(feature = "with-actix-web") {
    "rinja_actix"
} else if cfg!(feature = "with-axum") {
    "rinja_axum"
} else if cfg!(feature = "with-rocket") {
    "rinja_rocket"
} else if cfg!(feature = "with-warp") {
    "rinja_warp"
} else {
    "rinja"
};
