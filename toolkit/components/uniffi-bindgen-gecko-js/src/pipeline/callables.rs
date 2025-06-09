/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use super::*;
use crate::ConcrrencyMode;

use anyhow::{anyhow, bail, Result};

pub fn pass(module: &mut Module) -> Result<()> {
    let async_wrappers = module.config.async_wrappers.clone();
    let module_name = module.name.clone();
    let crate_name = module.crate_name.clone();

    // Track unconfigured callables for later reporting
    let mut unconfigured_callables = Vec::new();

    // Check functions (these are standalone functions, never callback interface methods)
    module.try_visit_mut(|func: &mut Function| {
        handle_callable(
            &mut func.callable,
            &async_wrappers,
            &mut unconfigured_callables,
            &module_name,
        )
    })?;

    // Check interface methods - both regular interfaces and trait interfaces need configuration
    // Regular interfaces (without vtables): require explicit async/sync configuration
    // Trait interfaces (with vtables): support foreign implementation, configuration handled differently
    let mut interface_results = Vec::new();
    module.visit_mut(|int: &mut Interface| {
        if int.vtable.is_none() {
            // Regular interface - require explicit configuration
            for constructor in &mut int.constructors {
                let result = handle_callable(
                    &mut constructor.callable,
                    &async_wrappers,
                    &mut unconfigured_callables,
                    &module_name,
                );
                if let Err(e) = result {
                    interface_results.push(Err(e));
                }
            }
            for method in &mut int.methods {
                let result = handle_callable(
                    &mut method.callable,
                    &async_wrappers,
                    &mut unconfigured_callables,
                    &module_name,
                );
                if let Err(e) = result {
                    interface_results.push(Err(e));
                }
            }
        } else {
            // Interface with vtable (trait interface) - apply default logic
            for method in &mut int.methods {
                let result = handle_vtable_callable(
                    &mut method.callable,
                    &async_wrappers,
                    &mut unconfigured_callables,
                    &module_name,
                    &int.name,
                );
                if let Err(e) = result {
                    interface_results.push(Err(e));
                }
            }
        }
    });

    // Check if any interface processing failed
    for result in interface_results {
        result?;
    }

    // Report unconfigured callables
    if !unconfigured_callables.is_empty() {
        let mut message = format!(
            "Found {} callables in module '{}' without explicit async/sync configuration in config.toml:\n",
            unconfigured_callables.len(),
            module_name
        );

        for (spec, info) in &unconfigured_callables {
            message.push_str(&format!("  - {}: {}\n", spec, info));
        }

        message.push_str(
            "\nPlease add these callables to the `toolkit/components/uniffi-bindgen-gecko-js/config.toml` file with explicit configuration:\n",
        );
        message.push_str(&format!("[{}.async_wrappers]\n", crate_name));

        for (spec, _) in &unconfigured_callables {
            message.push_str(&format!("\"{}\" = \"AsyncWrapped\"  # or \"Sync\"\n", spec));
        }

        // Fail the build with a helpful error message
        return Err(anyhow!(message));
    }

    Ok(())
}
fn handle_callable(
    callable: &mut Callable,
    async_wrappers: &indexmap::IndexMap<String, ConcrrencyMode>,
    unconfigured_callables: &mut Vec<(String, String)>,
    module_name: &str,
) -> Result<()> {
    let name = &callable.name;
    let spec = match &callable.kind {
        CallableKind::Function => name.clone(),
        CallableKind::Method { interface_name, .. }
        | CallableKind::Constructor { interface_name, .. } => {
            format!("{interface_name}.{name}")
        }
        CallableKind::VTableMethod { trait_name } => {
            bail!("Callback Interface Methods should be handled by handle_vtable_callable: {trait_name}.{name}")
        }
    };

    if callable.async_data.is_some() {
        callable.is_js_async = true;
        callable.uniffi_scaffolding_method = "UniFFIScaffolding.callAsync".to_string();

        // Check that there isn't an entry in async_wrappers for truly async methods
        if async_wrappers.contains_key(&spec) {
            bail!(
                "Method '{}' has async_data and should not have an entry in async_wrappers config",
                spec
            );
        }

        return Ok(());
    }

    // Check if this callable has explicit configuration
    match async_wrappers.get(&spec) {
        Some(ConcrrencyMode::Sync) => {
            callable.is_js_async = false;
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callSync".to_string();
        }
        Some(ConcrrencyMode::AsyncWrapped) => {
            callable.is_js_async = true;
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callAsyncWrapper".to_string();
        }
        None => {
            // Store information about the unconfigured callable
            let source_info = match &callable.kind {
                CallableKind::Function => {
                    format!("Function '{}' in module '{}'", name, module_name)
                }
                CallableKind::Method { interface_name, .. } => format!(
                    "Method '{}.{}' in module '{}'",
                    interface_name, name, module_name
                ),
                CallableKind::Constructor { interface_name, .. } => format!(
                    "Constructor '{}.{}' in module '{}'",
                    interface_name, name, module_name
                ),
                CallableKind::VTableMethod { trait_name } => format!(
                    "VTable method '{}.{}' in module '{}'",
                    trait_name, name, module_name
                ),
            };

            unconfigured_callables.push((spec.clone(), source_info));

            // Default to async for now - this won't matter if we fail the build
            callable.is_js_async = true;
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callAsyncWrapper".to_string();
        }
    }

    Ok(())
}

fn handle_vtable_callable(
    callable: &mut Callable,
    async_wrappers: &indexmap::IndexMap<String, ConcrrencyMode>,
    unconfigured_callables: &mut Vec<(String, String)>,
    module_name: &str,
    interface_name: &str,
) -> Result<()> {
    // Generate spec for vtable methods
    let name = &callable.name;
    let spec = format!("{}.{}", interface_name, name);

    // Check if this is a truly async method (has async_data)
    if callable.async_data.is_some() {
        // Check that there isn't an entry in async_wrappers for truly async methods
        if async_wrappers.contains_key(&spec) {
            bail!("VTable method '{}' has async_data and should not have an entry in async_wrappers config", spec);
        }
        callable.is_js_async = true;
        callable.uniffi_scaffolding_method = "UniFFIScaffolding.callAsync".to_string();
        return Ok(());
    }

    // Check if this vtable method has explicit configuration
    match async_wrappers.get(&spec) {
        Some(ConcrrencyMode::Sync) => {
            callable.is_js_async = false;
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callSync".to_string();
        }
        Some(ConcrrencyMode::AsyncWrapped) => {
            // Foreign-implemented trait interfaces can't be async-wrapped
            return Err(anyhow!(
                "VTable method '{}' cannot be AsyncWrapped as foreign-implemented trait interfaces don't support async wrapping",
                spec
            ));
        }
        None => {
            // Only add to unconfigured list if not configured
            let source_info = format!(
                "VTable method '{}.{}' in module '{}'",
                interface_name, name, module_name
            );
            unconfigured_callables.push((spec, source_info));
        }
    }

    Ok(())
}
