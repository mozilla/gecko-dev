/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::mock::{mock_key, MockKey};
pub use std::env::VarError;
use std::ffi::{OsStr, OsString};

mock_key! {
    pub struct MockCurrentExe => std::path::PathBuf
}

mock_key! {
    pub struct MockEnv(pub OsString) => String
}

mock_key! {
    pub struct MockHomeDir => super::path::PathBuf
}

pub struct ArgsOs {
    argv0: Option<OsString>,
}

impl Iterator for ArgsOs {
    type Item = OsString;

    fn next(&mut self) -> Option<Self::Item> {
        Some(
            self.argv0
                .take()
                .expect("only argv[0] is available when mocked"),
        )
    }
}

pub fn var<K: AsRef<OsStr>>(key: K) -> Result<String, VarError> {
    MockEnv(key.as_ref().to_os_string())
        .try_get(|value| value.clone())
        .ok_or(VarError::NotPresent)
}

pub fn var_os<K: AsRef<OsStr>>(key: K) -> Option<OsString> {
    MockEnv(key.as_ref().to_os_string()).try_get(|value| value.clone().into())
}

pub fn args_os() -> ArgsOs {
    MockCurrentExe.get(|r| ArgsOs {
        argv0: Some(r.clone().into()),
    })
}

pub fn home_dir() -> Option<super::path::PathBuf> {
    MockHomeDir.try_get(|p| p.clone())
}

#[allow(unused)]
pub fn current_exe() -> std::io::Result<super::path::PathBuf> {
    Ok(MockCurrentExe.get(|r| r.clone().into()))
}
