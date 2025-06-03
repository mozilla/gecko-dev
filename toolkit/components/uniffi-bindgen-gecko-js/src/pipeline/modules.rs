/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use crate::Config;
use std::collections::{HashMap, HashSet};

pub fn pass(mut config_map: HashMap<String, Config>) -> impl FnMut(&mut Module) -> Result<()> {
    move |module: &mut Module| {
        module.config = config_map.remove(&module.crate_name).unwrap_or_default();
        // Set the `fixture` flag for modules that belong to the text fixtures
        // To do this in a general way, we would need to remember which `Metadata::Namespace` items
        // came from the fixtures library and create a closure that captured that set.
        //
        // However, since we only have two fixture modules, we can do this very easily.
        if module.name.starts_with("uniffi_bindings_tests") {
            module.fixture = true;
        }

        // Workaround for the fact that the UniFFI 0.29.2 doesn't set this correctly.
        module.string_type_node = TypeNode {
            ty: Type::String,
            canonical_name: "String".to_string(),
            ..TypeNode::default()
        };

        // Collect all imports first, then process custom types
        let config = module.config.clone();
        let mut all_imports = HashSet::new();

        // Process custom types and collect imports
        module.try_visit_mut(|custom: &mut CustomType| {
            if let Some(custom_config) = config.custom_types.get(&custom.name) {
                custom.type_name = custom_config.type_name.clone();
                custom.lift_expr = Some(custom_config.lift.clone());
                custom.lower_expr = Some(custom_config.lower.clone());

                // Pre-render expressions with "builtinVal" identifier
                custom.lift_expr = Some(custom_config.lift.replace("{}", "builtinVal"));
                custom.lower_expr = Some(custom_config.lower.replace("{}", "value"));

                // Collect imports at module level instead of per-type
                all_imports.extend(custom_config.imports.iter().cloned());
            }
            Ok(())
        })?;

        let mut imports_vec: Vec<String> = all_imports.into_iter().collect();
        imports_vec.sort(); // For deterministic output
        module.imports = imports_vec;

        Ok(())
    }
}
