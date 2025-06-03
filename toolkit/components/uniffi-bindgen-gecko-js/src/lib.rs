/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs;
use std::io::Write;

use anyhow::{Context, Result};
use askama::Template;
use camino::{Utf8Path, Utf8PathBuf};
use clap::Parser;
use indexmap::{IndexMap, IndexSet};
use serde::{Deserialize, Serialize};
use uniffi_bindgen::pipeline::initial;
use uniffi_pipeline::PrintOptions;

mod config_supplier;
pub mod pipeline;

use config_supplier::GeckoJsCrateConfigSupplier;
use pipeline::{gecko_js_pipeline, GeckoPipeline};
use uniffi_pipeline::Node;

#[derive(Debug, Parser)]
#[clap(name = "uniffi-bindgen-gecko-js")]
#[clap(version = clap::crate_version!())]
#[clap(about = "JS bindings generator for Rust")]
#[clap(propagate_version = true)]
struct CliArgs {
    // This is a really convoluted set of arguments, but we're only expecting to be called by
    // `mach_commands.py`
    #[clap(long, value_name = "FILE")]
    library_path: Utf8PathBuf,

    #[clap(long, value_name = "FILE")]
    fixtures_library_path: Utf8PathBuf,

    #[clap(subcommand)]
    command: Command,
}

#[derive(clap::Subcommand, Debug, Clone)]
enum Command {
    /// Generate new C++ and JS bindings
    Generate(GenerateArgs),
    /// Execute the UniFFI pipeline for debugging purposes
    Pipeline(PipelineArgs),
}

#[derive(clap::Args, Debug, Clone)]
struct GenerateArgs {
    #[clap(long, value_name = "FILE")]
    js_dir: Utf8PathBuf,
    #[clap(long, value_name = "FILE")]
    fixture_js_dir: Utf8PathBuf,
    #[clap(long, value_name = "FILE")]
    cpp_path: Utf8PathBuf,
    #[clap(long, value_name = "FILE")]
    docs_path: Utf8PathBuf,
}

#[derive(clap::Args, Debug, Clone)]
struct PipelineArgs {
    /// Only show passes that match <PASS>
    ///
    /// Use `last` to only show the last pass, this can be useful when you're writing new pipelines
    #[clap(short, long)]
    pass: Option<String>,

    /// Only show data for types with name <FILTER_TYPE>
    #[clap(short = 't', long = "type")]
    filter_type: Option<String>,

    /// Only show data for items with fields that match <FILTER>
    #[clap(short = 'n', long = "name")]
    filter_name: Option<String>,

    /// Only show data for items with fields that match <FILTER>
    #[clap(long)]
    no_diff: bool,
}

/// Configuration for a single Component
#[derive(Clone, Debug, Deserialize, Serialize, Node)]
pub struct Config {
    #[serde(default)]
    async_wrappers: AsyncWrappersConfig,
    #[serde(default)]
    custom_types: IndexMap<String, CustomTypeConfig>,
}

#[derive(Clone, Debug, Deserialize, Serialize, Node)]
struct AsyncWrappersConfig {
    /// This converts synchronous Rust functions into async JS functions, by wrapping them at the
    /// C++ layer.
    #[serde(default)]
    enable: bool,
    /// Functions that should be run on the main thread and not be wrapped
    #[serde(default)]
    main_thread: IndexSet<String>,
}

#[derive(Clone, Debug, Deserialize, Serialize, Node)]
struct CustomTypeConfig {
    /// The name of the type in JavaScript
    #[serde(default)]
    type_name: Option<String>,
    /// Modules to import (e.g., for Node.js require statements)
    #[serde(default)]
    imports: Vec<String>,
    /// Expression to convert from FFI type to JS type
    /// {} will be replaced with the value
    lift: String,
    /// Expression to convert from JS type to FFI type
    lower: String,
}

fn render(out_path: &Utf8Path, template: impl Template) -> Result<()> {
    let contents = template.render()?;
    let mut f = fs::File::create(out_path)
        .context(format!("Failed to create file {:?}", out_path.file_name()))?;
    writeln!(f, "{}", contents).context(format!("Failed to write file {}", out_path))?;
    println!("Generated: {out_path}");
    Ok(())
}

pub fn run_main() -> Result<()> {
    let args = CliArgs::parse();
    let (root, pipeline) = root_and_pipeline(args.library_path, args.fixtures_library_path)?;

    match args.command {
        Command::Generate(generate_args) => run_generate(root, pipeline, generate_args),
        Command::Pipeline(pipeline_args) => run_pipeline(root, pipeline, pipeline_args),
    }
}

fn run_generate(
    root: initial::Root,
    mut pipeline: GeckoPipeline,
    args: GenerateArgs,
) -> Result<()> {
    let root = pipeline.execute(root)?;
    render(&args.cpp_path, root.cpp_scaffolding)?;
    for module in root.modules.values() {
        let dir = if module.fixture {
            &args.fixture_js_dir
        } else {
            &args.js_dir
        };
        render(&dir.join(&module.js_filename), module)?;
    }
    for entry in fs::read_dir(&args.docs_path)? {
        let path = entry?.path();
        if path.file_name().unwrap() != "index.md" {
            fs::remove_file(path)?;
        }
    }
    for module_docs in root.module_docs.iter() {
        render(&args.docs_path.join(&module_docs.filename), module_docs)?;
    }
    Ok(())
}

fn run_pipeline(
    root: initial::Root,
    mut pipeline: GeckoPipeline,
    args: PipelineArgs,
) -> Result<()> {
    let opts = PrintOptions {
        pass: args.pass,
        no_diff: args.no_diff,
        filter_type: args.filter_type,
        filter_name: args.filter_name,
    };
    pipeline.print_passes(root, opts)
}

fn root_and_pipeline(
    library_path: Utf8PathBuf,
    fixtures_library_path: Utf8PathBuf,
) -> Result<(initial::Root, GeckoPipeline)> {
    let config_supplier = GeckoJsCrateConfigSupplier::new()?;
    let root = initial::Root::from_library(&config_supplier, &library_path, None)?;
    let fixtures_root =
        initial::Root::from_library(&config_supplier, &fixtures_library_path, None)?;
    let root = initial::Root {
        modules: root
            .modules
            .into_iter()
            .chain(fixtures_root.modules)
            .collect(),
        cdylib: None,
    };
    let pipeline = gecko_js_pipeline(toml::from_str(include_str!("../config.toml"))?);
    Ok((root, pipeline))
}
