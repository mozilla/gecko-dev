/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::{
    fs::{self, File},
    io,
    path::PathBuf,
};

/// Create a string literal to be used as an environment variable name.
///
/// This adds the capitalized package name as the prefix to the environment
/// variable.
macro_rules! ekey {
    ( $name:literal ) => {
        concat!(env!("CARGO_PKG_NAME"), "_", $name).to_uppercase()
    };
}

/// Initialize logging and place the logging file in
pub(crate) fn init(user_app_data_dir: Option<PathBuf>) {
    let log_target = make_log_target(user_app_data_dir);

    env_logger::builder()
        .filter_level(log::LevelFilter::Warn)
        .parse_env(
            env_logger::Env::new()
                .filter(ekey!("_LOG"))
                .write_style(ekey!("_LOG_STYLE")),
        )
        .target(env_logger::fmt::Target::Pipe(log_target))
        .init();
}

fn make_log_target(log_directory: Option<PathBuf>) -> Box<dyn io::Write + Send + 'static> {
    if let Some(log_directory) = log_directory.map(|path| path.join("Crash Reports")) {
        if fs::create_dir_all(&log_directory).is_ok() {
            let log_path = log_directory.join(concat!(env!("CARGO_PKG_NAME"), ".log"));
            if let Ok(log) = File::create(log_path) {
                return Box::new(log);
            }
        }
    }

    Box::new(io::stderr())
}
