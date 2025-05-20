/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{anyhow, Context, Result};
use cargo_metadata::MetadataCommand;
use toml::{map::Map, value::Table, Value};
use uniffi_bindgen::{cargo_metadata::CrateConfigSupplier, BindgenCrateConfigSupplier};

/// Responsible for finding UDL files and config values for crates
pub struct GeckoJsCrateConfigSupplier {
    // Used to lookup the UDL files
    cargo_crate_config_supplier: CrateConfigSupplier,
    // Used to get config values
    config_table: Map<String, Value>,
}

impl GeckoJsCrateConfigSupplier {
    pub fn new() -> Result<Self> {
        Ok(Self {
            cargo_crate_config_supplier: MetadataCommand::new()
                .exec()
                .context("error running cargo metadata")?
                .into(),
            config_table: toml::from_str(include_str!("../config.toml"))?,
        })
    }
}

impl BindgenCrateConfigSupplier for &GeckoJsCrateConfigSupplier {
    fn get_udl(&self, crate_name: &str, udl_name: &str) -> Result<String> {
        self.cargo_crate_config_supplier
            .get_udl(crate_name, udl_name)
    }

    fn get_toml(&self, crate_name: &str) -> Result<Option<Table>> {
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
