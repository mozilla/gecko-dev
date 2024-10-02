/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Manage the universe of ComponentInterfaces / Configs
//!
//! uniffi-bindgen-gecko-js is unique because it generates bindings over a set of UDL files rather
//! than just one.  This is because we want to generate the WebIDL statically rather than generate
//! it.  To accomplish that, each WebIDL function inputs an opaque integer id that identifies which
//! version of it should run, for example `CallSync` inputs a function id.  Operating on all UDL
//! files at once simplifies the task of ensuring those ids are to be unique and consistent between
//! the JS and c++ code.
//!
//! This module manages the list of ComponentInterface and the object ids.

use std::collections::{BTreeSet, HashMap, HashSet};

use anyhow::{anyhow, bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use uniffi_bindgen::interface::{CallbackInterface, ComponentInterface, FfiFunction, Object};

use crate::render::cpp::exposed_functions;
use crate::Component;

pub struct ComponentUniverse {
    pub components: Vec<Component>,
    pub fixture_components: Vec<Component>,
}

impl ComponentUniverse {
    pub fn new(library_path: Utf8PathBuf, fixtures_library_path: Utf8PathBuf) -> Result<Self> {
        let config_supplier = GeckoJsCrateConfigSupplier::new()?;
        let universe = Self {
            components: find_components(&library_path, &config_supplier)?,
            fixture_components: find_components(&fixtures_library_path, &config_supplier)?,
        };
        universe.check_udl_namespaces_unique()?;
        universe.check_callback_interfaces()?;
        Ok(universe)
    }

    fn check_udl_namespaces_unique(&self) -> Result<()> {
        let mut set = HashSet::new();
        for ci in self.iter_cis() {
            if !set.insert(ci.namespace()) {
                bail!("UDL files have duplicate namespace: {}", ci.namespace());
            }
        }
        Ok(())
    }

    fn check_callback_interfaces(&self) -> Result<()> {
        // We don't currently support callback interfaces returning values or throwing errors.
        for ci in self.iter_cis() {
            for cbi in ci.callback_interface_definitions() {
                for method in cbi.methods() {
                    if method.return_type().is_some() {
                        bail!("Callback interface method {}.{} throws an error, which is not yet supported", cbi.name(), method.name())
                    }
                    if method.throws_type().is_some() {
                        bail!("Callback interface method {}.{} returns a value, which is not yet supported", cbi.name(), method.name())
                    }
                }
            }
        }
        Ok(())
    }

    pub fn iter_components(&self) -> impl Iterator<Item = &Component> {
        self.components.iter().chain(self.fixture_components.iter())
    }

    pub fn iter_cis(&self) -> impl Iterator<Item = &ComponentInterface> {
        self.iter_components().map(|component| &component.ci)
    }
}

fn find_components(
    library_path: &Utf8Path,
    config_supplier: &GeckoJsCrateConfigSupplier,
) -> Result<Vec<Component>> {
    let mut components = uniffi_bindgen::find_components(library_path, config_supplier)?
        .into_iter()
        // FIXME(Bug 1913982): Need to filter out components that use callback interfaces for now
        .filter(|component| {
            let namespace = component.ci.namespace();
            namespace != "errorsupport" && namespace != "fixture_callbacks"
        })
        .map(|component| {
            Ok(Component {
                config: toml::Value::Table(component.config).try_into()?,
                ci: component.ci,
            })
        })
        .collect::<Result<Vec<Component>>>()?;
    // Sort components entries to ensure consistent output
    components.sort_by(|c1, c2| c1.ci.namespace().cmp(c2.ci.namespace()));
    Ok(components)
}

/// Responsible for finding UDL files and config values for crates
struct GeckoJsCrateConfigSupplier {
    // Used to lookup the UDL files
    cargo_crate_config_supplier: uniffi_bindgen::cargo_metadata::CrateConfigSupplier,
    // Used to get config values
    config_table: toml::map::Map<String, toml::Value>,
}

impl GeckoJsCrateConfigSupplier {
    fn new() -> Result<Self> {
        Ok(Self {
            cargo_crate_config_supplier: cargo_metadata::MetadataCommand::new()
                .exec()
                .context("error running cargo metadata")?
                .into(),
            config_table: toml::from_str(include_str!("../config.toml"))?,
        })
    }
}

impl uniffi_bindgen::BindgenCrateConfigSupplier for GeckoJsCrateConfigSupplier {
    fn get_udl(&self, crate_name: &str, udl_name: &str) -> anyhow::Result<String> {
        self.cargo_crate_config_supplier
            .get_udl(crate_name, udl_name)
    }

    fn get_toml(&self, crate_name: &str) -> anyhow::Result<Option<toml::value::Table>> {
        self.config_table
            .get(crate_name)
            .map(|v| {
                v.as_table()
                    .ok_or_else(|| anyhow!("Config value not table"))
                    .cloned()
            })
            .transpose()
    }
}

pub struct FunctionIds<'a> {
    // Map (CI namespace, func name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> FunctionIds<'a> {
    pub fn new(cis: &'a ComponentUniverse) -> Self {
        Self {
            map: cis
                .iter_cis()
                .flat_map(|ci| exposed_functions(ci).map(move |f| (ci.namespace(), f.name())))
                .enumerate()
                .map(|(i, (namespace, name))| ((namespace, name), i))
                // Sort using BTreeSet to guarantee the IDs remain stable across runs
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect(),
        }
    }

    pub fn get(&self, ci: &ComponentInterface, func: &FfiFunction) -> usize {
        return *self.map.get(&(ci.namespace(), func.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, func: &FfiFunction) -> String {
        format!("{}:{}", ci.namespace(), func.name())
    }
}

pub struct ObjectIds<'a> {
    // Map (CI namespace, object name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> ObjectIds<'a> {
    pub fn new(cis: &'a ComponentUniverse) -> Self {
        Self {
            map: cis
                .iter_cis()
                .flat_map(|ci| {
                    ci.object_definitions()
                        .iter()
                        .map(move |o| (ci.namespace(), o.name()))
                })
                .enumerate()
                .map(|(i, (namespace, name))| ((namespace, name), i))
                // Sort using BTreeSet to guarantee the IDs remain stable across runs
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect(),
        }
    }

    pub fn get(&self, ci: &ComponentInterface, obj: &Object) -> usize {
        return *self.map.get(&(ci.namespace(), obj.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, obj: &Object) -> String {
        format!("{}:{}", ci.namespace(), obj.name())
    }
}

pub struct CallbackIds<'a> {
    // Map (CI namespace, callback name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> CallbackIds<'a> {
    pub fn new(cis: &'a ComponentUniverse) -> Self {
        Self {
            map: cis
                .iter_cis()
                .flat_map(|ci| {
                    ci.callback_interface_definitions()
                        .iter()
                        .map(move |cb| (ci.namespace(), cb.name()))
                })
                .enumerate()
                .map(|(i, (namespace, name))| ((namespace, name), i))
                // Sort using BTreeSet to guarantee the IDs remain stable across runs
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect(),
        }
    }

    pub fn get(&self, ci: &ComponentInterface, cb: &CallbackInterface) -> usize {
        return *self.map.get(&(ci.namespace(), cb.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, cb: &CallbackInterface) -> String {
        format!("{}:{}", ci.namespace(), cb.name())
    }
}
