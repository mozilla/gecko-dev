/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use std::borrow::Cow;

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|module: &mut Module| {
        module.js_docstring = format_docstring(module.docstring.as_ref().unwrap_or(&module.name));
    });
    root.visit_mut(|func: &mut Function| {
        func.js_docstring = format_callable_docstring(
            &func.callable,
            func.docstring.as_ref().unwrap_or(&func.name),
        );
    });
    root.visit_mut(|meth: &mut Method| {
        meth.js_docstring = format_callable_docstring(
            &meth.callable,
            meth.docstring.as_ref().unwrap_or(&meth.name),
        );
    });
    root.visit_mut(|cons: &mut Constructor| {
        cons.js_docstring = format_callable_docstring(
            &cons.callable,
            cons.docstring.as_ref().unwrap_or(&cons.name),
        );
    });
    root.visit_mut(|rec: &mut Record| {
        rec.js_docstring = format_docstring(rec.docstring.as_ref().unwrap_or(&rec.name));
    });
    root.visit_mut(|en: &mut Enum| {
        en.js_docstring = format_docstring(en.docstring.as_ref().unwrap_or(&en.name));
    });
    root.visit_mut(|variant: &mut Variant| {
        variant.js_docstring =
            format_docstring(variant.docstring.as_ref().unwrap_or(&variant.name));
    });
    root.visit_mut(|field: &mut Field| {
        field.js_docstring = format_docstring(field.docstring.as_ref().unwrap_or(&field.name));
    });
    root.visit_mut(|int: &mut Interface| {
        int.js_docstring = format_docstring(int.docstring.as_ref().unwrap_or(&int.name));
    });
    root.visit_mut(|cbi: &mut CallbackInterface| {
        cbi.js_docstring = format_docstring(cbi.docstring.as_ref().unwrap_or(&cbi.name));
    });
    root.visit_mut(|custom: &mut CustomType| {
        custom.js_docstring = format_docstring(custom.docstring.as_ref().unwrap_or(&custom.name));
    });
    root.visit_mut(|field: &mut Field| {
        field.js_docstring = format_docstring(field.docstring.as_ref().unwrap_or(&field.name));
    });
    Ok(())
}

/// Format a docstring for the JS code
fn format_docstring(docstring: &str) -> String {
    // Remove any existing indentation
    let docstring = textwrap::dedent(docstring);
    // "Escape" `*/` chars to avoid closing the comment
    let docstring = docstring.replace("*/", "* /");
    // Format the docstring making sure to:
    //   - Start with `/**` and end with `*/`
    //   - Line up all the `*` chars correctly
    //   - Add trailing leading spaces, to make this work with the `{{ -}}` tag
    let mut output = String::default();
    output.push_str("/**\n");
    for line in docstring.split('\n') {
        output.push_str(" * ");
        output.push_str(line);
        output.push('\n');
    }
    output.push_str(" */");
    output
}

/// Format a docstring for a function/method
fn format_callable_docstring(callable: &Callable, docstring: &str) -> String {
    let mut parts = vec![Cow::from(docstring)];
    for arg in callable.arguments.iter() {
        let type_name = &arg.ty.jsdoc_name;
        let arg_name = &arg.name;
        parts.push(format!("@param {{{type_name}}} {arg_name}").into());
    }
    if let Some(return_ty) = &callable.return_type.ty {
        let type_name = &return_ty.jsdoc_name;
        parts.push(if callable.is_js_async {
            format!("@returns {{Promise<{type_name}>}}}}").into()
        } else {
            format!("@returns {{{type_name}}}").into()
        });
    }
    format_docstring(&parts.join("\n"))
}
