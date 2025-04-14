// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    io::Write as _,
    sync::{Once, OnceLock},
    time::{Duration, Instant},
};

use env_logger::Builder;

fn since_start() -> Duration {
    static START_TIME: OnceLock<Instant> = OnceLock::new();
    START_TIME.get_or_init(Instant::now).elapsed()
}

pub fn init(level_filter: Option<log::LevelFilter>) {
    static INIT_ONCE: Once = Once::new();

    if ::log::STATIC_MAX_LEVEL == ::log::LevelFilter::Off {
        return;
    }

    INIT_ONCE.call_once(|| {
        let mut builder = Builder::from_env("RUST_LOG");
        if let Some(filter) = level_filter {
            builder.filter_level(filter);
        }
        builder.format(|buf, record| {
            let elapsed = since_start();
            writeln!(
                buf,
                "{}.{:03} {} {}",
                elapsed.as_secs(),
                elapsed.as_millis() % 1000,
                record.level(),
                record.args()
            )
        });
        if let Err(e) = builder.try_init() {
            eprintln!("Logging initialization error {e:?}");
        } else {
            ::log::debug!("Logging initialized");
        }
    });
}

#[macro_export]
// TODO: Enable `#[clippy::format_args]` once our MSRV is >= 1.84
macro_rules! qerror {
    ($($arg:tt)*) => ( {
        #[cfg(any(test, feature = "bench"))]
        ::neqo_common::log::init(None);
        ::log::error!($($arg)*);
    } );
}
#[macro_export]
// TODO: Enable `#[clippy::format_args]` once our MSRV is >= 1.84
macro_rules! qwarn {
    ($($arg:tt)*) => ( {
        #[cfg(any(test, feature = "bench"))]
        ::neqo_common::log::init(None);
        ::log::warn!($($arg)*);
    } );
}
#[macro_export]
// TODO: Enable `#[clippy::format_args]` once our MSRV is >= 1.84
macro_rules! qinfo {
    ($($arg:tt)*) => ( {
        #[cfg(any(test, feature = "bench"))]
        ::neqo_common::log::init(None);
        ::log::info!($($arg)*);
    } );
}
#[macro_export]
// TODO: Enable `#[clippy::format_args]` once our MSRV is >= 1.84
macro_rules! qdebug {
    ($($arg:tt)*) => ( {
        #[cfg(any(test, feature = "bench"))]
        ::neqo_common::log::init(None);
        ::log::debug!($($arg)*);
    } );
}
#[macro_export]
// TODO: Enable `#[clippy::format_args]` once our MSRV is >= 1.84
macro_rules! qtrace {
    ($($arg:tt)*) => ( {
        #[cfg(any(test, feature = "bench"))]
        ::neqo_common::log::init(None);
        ::log::trace!($($arg)*);
    } );
}
