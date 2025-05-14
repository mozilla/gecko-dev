/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
mod nodes;
mod callables;
mod callback_interfaces;
mod cpp_callback_interfaces;
mod cpp_ffi_definitions;
mod cpp_ffi_types;
mod cpp_names;
mod cpp_scaffolding_calls;
mod docs;
mod enums;
mod interfaces;
mod js_docstrings;
mod js_filename;
mod js_names;
mod literals;
mod modules;
mod types;

use crate::Config;
use anyhow::{bail, Result};
pub use nodes::*;
use std::collections::HashMap;
use uniffi_bindgen::pipeline::{general, initial};
use uniffi_pipeline::{Node, Pipeline};

pub type GeckoPipeline = Pipeline<initial::Root, Root>;

pub fn gecko_js_pipeline(pipeline_map: HashMap<String, Config>) -> GeckoPipeline {
    general::pipeline()
        .convert_ir_pass::<Root>()
        .pass(modules::pass(pipeline_map))
        .pass(enums::pass)
        .pass(callables::pass)
        .pass(interfaces::pass)
        .pass(callback_interfaces::pass)
        .pass(js_filename::pass)
        .pass(js_names::pass)
        .pass(js_docstrings::pass)
        .pass(cpp_ffi_definitions::pass)
        .pass(cpp_scaffolding_calls::pass)
        .pass(cpp_callback_interfaces::pass)
        .pass(types::pass)
        .pass(cpp_ffi_types::pass)
        .pass(cpp_names::pass)
        .pass(literals::pass)
        .pass(docs::pass)
}
