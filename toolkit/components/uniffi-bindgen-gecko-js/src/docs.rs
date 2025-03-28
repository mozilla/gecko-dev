/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs;

use anyhow::Context;
use askama::Template;
use camino::Utf8Path;
use uniffi_bindgen::ComponentInterface;

use crate::{render::js::*, Component, Result};

pub fn render_docs(docs_dir: &Utf8Path, components: &[Component]) -> Result<()> {
    for entry in fs::read_dir(docs_dir.as_std_path()).unwrap() {
        let entry = entry?;
        if entry.file_name() == "index.md" {
            continue;
        }
        fs::remove_file(entry.path())?;
    }

    for c in components {
        let namespace = c.ci.namespace();
        let path = docs_dir.join(format!("{namespace}.md"));
        let module_name = js_module_name(namespace);
        // The JS doc modulename chops of the trailing `.mjs`, but not the `.sys`
        let jsdoc_module_name = module_name
            .strip_suffix(".mjs")
            .context("js_module_name does not end with `.mjs`")?
            .to_string();
        let template = ApiDocTemplate {
            module_name,
            jsdoc_module_name,
            classes: classes(&c.ci),
            functions: functions(&c.ci),
        };
        if !template.is_empty() {
            fs::write(path, template.render()?)?;
        }
    }
    Ok(())
}

fn classes(ci: &ComponentInterface) -> Vec<String> {
    std::iter::empty()
        .chain(
            ci.object_definitions()
                .into_iter()
                .filter(|o| o.docstring().is_some())
                .map(|o| o.js_name()),
        )
        .chain(
            ci.record_definitions()
                .into_iter()
                .filter(|r| r.docstring().is_some())
                .map(|r| r.js_name()),
        )
        .chain(
            ci.enum_definitions()
                .into_iter()
                .filter(|e| e.docstring().is_some())
                .map(|e| e.js_name()),
        )
        .chain(
            ci.enum_definitions()
                .into_iter()
                .filter(|e| !e.is_flat())
                .flat_map(|e| {
                    e.variants().into_iter().filter_map(move |v| {
                        if ci.is_name_used_as_error(e.name()) {
                            Some(v.js_name(e.is_flat()))
                        } else {
                            None
                            //format!("{}.{}", e.js_name(), v.js_name(e.is_flat()))
                        }
                    })
                }),
        )
        // TODO
        //.chain(ci.callback_interface_definitions().into_iter().filter(|c| c.docstring().is_some().map(|c| c.js_name()))
        .collect()
}

fn functions(ci: &ComponentInterface) -> Vec<String> {
    ci.function_definitions()
        .into_iter()
        .filter(|f| f.docstring().is_some())
        .map(|f| f.js_name())
        .collect()
}

#[derive(Template)]
#[template(path = "api-doc.md", escape = "none")]
pub struct ApiDocTemplate {
    jsdoc_module_name: String,
    module_name: String,
    classes: Vec<String>,
    functions: Vec<String>,
}

impl ApiDocTemplate {
    fn is_empty(&self) -> bool {
        self.classes.is_empty() && self.functions.is_empty()
    }
}
