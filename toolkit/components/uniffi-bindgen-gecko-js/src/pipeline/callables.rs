/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;

pub fn pass(module: &mut Module) -> Result<()> {
    let async_wrappers = module.config.async_wrappers.clone();
    module.visit_mut(|callable: &mut Callable| {
        if callable.async_data.is_some() {
            callable.is_js_async = true;
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callAsync".to_string();
            return;
        }

        let name = &callable.name;
        let spec = match &callable.kind {
            CallableKind::Function => name.clone(),
            CallableKind::Method { interface_name, .. }
            | CallableKind::VTableMethod {
                trait_name: interface_name,
            }
            | CallableKind::Constructor { interface_name, .. } => {
                format!("{interface_name}.{name}")
            }
        };
        if async_wrappers.enable && !async_wrappers.main_thread.contains(&spec) {
            callable.is_js_async = true;
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callAsyncWrapper".to_string();
        } else {
            callable.uniffi_scaffolding_method = "UniFFIScaffolding.callSync".to_string();
        }
    });
    Ok(())
}
