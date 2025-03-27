use std::{
    ffi::{OsStr, OsString},
    fmt::{self, Display},
    iter::once,
    process::Command,
};

use format::lazy_format;
use miette::{ensure, Context, IntoDiagnostic};

pub(crate) fn which(name: &'static str, desc: &str) -> miette::Result<OsString> {
    let found = ::which::which(name)
        .into_diagnostic()
        .wrap_err(lazy_format!("failed to find `{name}` executable"))?;
    log::debug!("using {desc} from {}", found.display());
    Ok(found.file_name().unwrap().to_owned())
}

pub(crate) struct EasyCommand {
    inner: Command,
}

impl EasyCommand {
    pub(crate) fn new<C>(cmd: C, f: impl FnOnce(&mut Command) -> &mut Command) -> Self
    where
        C: AsRef<OsStr>,
    {
        let mut cmd = Command::new(cmd);
        f(&mut cmd);
        Self { inner: cmd }
    }

    pub(crate) fn spawn(&mut self) -> miette::Result<()> {
        log::debug!("spawning {self}…");
        let status = self
            .inner
            .spawn()
            .into_diagnostic()
            .wrap_err_with(|| format!("failed to spawn {self}"))?
            .wait()
            .into_diagnostic()
            .wrap_err_with(|| format!("failed to wait for exit code from {self}"))?;
        log::debug!("{self} returned {:?}", status.code());
        ensure!(status.success(), "{self} returned {:?}", status.code());
        Ok(())
    }
}

impl Display for EasyCommand {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let Self { inner } = self;
        let prog = inner.get_program().to_string_lossy();
        let args = inner.get_args().map(|a| a.to_string_lossy());
        let shell_words = ::shell_words::join(once(prog).chain(args));
        write!(f, "`{shell_words}`")
    }
}
