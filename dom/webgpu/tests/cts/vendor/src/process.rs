use std::ffi::OsString;

use miette::{Context, IntoDiagnostic};

pub(crate) fn which(name: &'static str, desc: &str) -> miette::Result<OsString> {
    let found = ::which::which(name)
        .into_diagnostic()
        .wrap_err_with(|| format!("failed to find `{name}` executable"))?;
    log::debug!("using {desc} from {}", found.display());
    Ok(found.file_name().unwrap().to_owned())
}
