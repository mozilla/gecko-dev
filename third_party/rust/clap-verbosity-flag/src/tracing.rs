// These re-exports of the tracing crate types it easy to use this crate without having to depend on
// the tracing crate directly. See <https://github.com/clap-rs/clap-verbosity-flag/issues/54> for
// more information.
pub use tracing_core::{Level, LevelFilter};

use crate::{LogLevel, Verbosity, VerbosityFilter};

impl From<VerbosityFilter> for LevelFilter {
    fn from(filter: VerbosityFilter) -> Self {
        match filter {
            VerbosityFilter::Off => LevelFilter::OFF,
            VerbosityFilter::Error => LevelFilter::ERROR,
            VerbosityFilter::Warn => LevelFilter::WARN,
            VerbosityFilter::Info => LevelFilter::INFO,
            VerbosityFilter::Debug => LevelFilter::DEBUG,
            VerbosityFilter::Trace => LevelFilter::TRACE,
        }
    }
}

impl From<LevelFilter> for VerbosityFilter {
    fn from(level: LevelFilter) -> Self {
        match level {
            LevelFilter::OFF => Self::Off,
            LevelFilter::ERROR => Self::Error,
            LevelFilter::WARN => Self::Warn,
            LevelFilter::INFO => Self::Info,
            LevelFilter::DEBUG => Self::Debug,
            LevelFilter::TRACE => Self::Trace,
        }
    }
}

impl From<VerbosityFilter> for Option<Level> {
    fn from(filter: VerbosityFilter) -> Self {
        match filter {
            VerbosityFilter::Off => None,
            VerbosityFilter::Error => Some(Level::ERROR),
            VerbosityFilter::Warn => Some(Level::WARN),
            VerbosityFilter::Info => Some(Level::INFO),
            VerbosityFilter::Debug => Some(Level::DEBUG),
            VerbosityFilter::Trace => Some(Level::TRACE),
        }
    }
}

impl From<Option<Level>> for VerbosityFilter {
    fn from(level: Option<Level>) -> Self {
        match level {
            None => Self::Off,
            Some(Level::ERROR) => Self::Error,
            Some(Level::WARN) => Self::Warn,
            Some(Level::INFO) => Self::Info,
            Some(Level::DEBUG) => Self::Debug,
            Some(Level::TRACE) => Self::Trace,
        }
    }
}

impl<L: LogLevel> From<Verbosity<L>> for LevelFilter {
    fn from(v: Verbosity<L>) -> Self {
        v.tracing_level_filter()
    }
}

impl<L: LogLevel> From<Verbosity<L>> for Option<Level> {
    fn from(v: Verbosity<L>) -> Self {
        v.tracing_level()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{DebugLevel, ErrorLevel, InfoLevel, OffLevel, TraceLevel, Verbosity, WarnLevel};

    #[test]
    fn tracing_level() {
        let v = Verbosity::<OffLevel>::default();
        assert_eq!(v.tracing_level(), None);
        assert_eq!(v.tracing_level_filter(), LevelFilter::OFF);

        let v = Verbosity::<ErrorLevel>::default();
        assert_eq!(v.tracing_level(), Some(Level::ERROR));
        assert_eq!(v.tracing_level_filter(), LevelFilter::ERROR);

        let v = Verbosity::<WarnLevel>::default();
        assert_eq!(v.tracing_level(), Some(Level::WARN));
        assert_eq!(v.tracing_level_filter(), LevelFilter::WARN);

        let v = Verbosity::<InfoLevel>::default();
        assert_eq!(v.tracing_level(), Some(Level::INFO));
        assert_eq!(v.tracing_level_filter(), LevelFilter::INFO);

        let v = Verbosity::<DebugLevel>::default();
        assert_eq!(v.tracing_level(), Some(Level::DEBUG));
        assert_eq!(v.tracing_level_filter(), LevelFilter::DEBUG);

        let v = Verbosity::<TraceLevel>::default();
        assert_eq!(v.tracing_level(), Some(Level::TRACE));
        assert_eq!(v.tracing_level_filter(), LevelFilter::TRACE);
    }

    #[test]
    fn into_opt_level() {
        let v = Verbosity::<OffLevel>::default();
        assert_eq!(Option::<Level>::from(v), None);

        let v = Verbosity::<ErrorLevel>::default();
        assert_eq!(Option::<Level>::from(v), Some(Level::ERROR));

        let v = Verbosity::<WarnLevel>::default();
        assert_eq!(Option::<Level>::from(v), Some(Level::WARN));

        let v = Verbosity::<InfoLevel>::default();
        assert_eq!(Option::<Level>::from(v), Some(Level::INFO));

        let v = Verbosity::<DebugLevel>::default();
        assert_eq!(Option::<Level>::from(v), Some(Level::DEBUG));

        let v = Verbosity::<TraceLevel>::default();
        assert_eq!(Option::<Level>::from(v), Some(Level::TRACE));
    }

    #[test]
    fn into_level_filter() {
        let v = Verbosity::<OffLevel>::default();
        assert_eq!(LevelFilter::from(v), LevelFilter::OFF);

        let v = Verbosity::<ErrorLevel>::default();
        assert_eq!(LevelFilter::from(v), LevelFilter::ERROR);

        let v = Verbosity::<WarnLevel>::default();
        assert_eq!(LevelFilter::from(v), LevelFilter::WARN);

        let v = Verbosity::<InfoLevel>::default();
        assert_eq!(LevelFilter::from(v), LevelFilter::INFO);

        let v = Verbosity::<DebugLevel>::default();
        assert_eq!(LevelFilter::from(v), LevelFilter::DEBUG);

        let v = Verbosity::<TraceLevel>::default();
        assert_eq!(LevelFilter::from(v), LevelFilter::TRACE);
    }
}
