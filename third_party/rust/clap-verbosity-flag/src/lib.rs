//! Control `log` level with a `--verbose` flag for your CLI
//!
//! # Examples
//!
//! To get `--quiet` and `--verbose` flags through your entire program, just `flatten`
//! [`Verbosity`]:
//! ```rust,no_run
//! # use clap::Parser;
//! # use clap_verbosity_flag::Verbosity;
//! #
//! # /// Le CLI
//! # #[derive(Debug, Parser)]
//! # struct Cli {
//! #[command(flatten)]
//! verbose: Verbosity,
//! # }
//! ```
//!
//! You can then use this to configure your logger:
//! ```rust,no_run
//! # use clap::Parser;
//! # use clap_verbosity_flag::Verbosity;
//! #
//! # /// Le CLI
//! # #[derive(Debug, Parser)]
//! # struct Cli {
//! #     #[command(flatten)]
//! #     verbose: Verbosity,
//! # }
//! let cli = Cli::parse();
//! # #[cfg(feature = "log")]
//! env_logger::Builder::new()
//!     .filter_level(cli.verbose.log_level_filter())
//!     .init();
//! ```
//!
//! By default, this will only report errors.
//! - `-q` silences output
//! - `-v` show warnings
//! - `-vv` show info
//! - `-vvv` show debug
//! - `-vvvv` show trace
//!
//! By default, the log level is set to Error. To customize this to a different level, pass a type
//! implementing the [`LogLevel`] trait to [`Verbosity`]:
//!
//! ```rust,no_run
//! # use clap::Parser;
//! use clap_verbosity_flag::{Verbosity, InfoLevel};
//!
//! /// Le CLI
//! #[derive(Debug, Parser)]
//! struct Cli {
//!     #[command(flatten)]
//!     verbose: Verbosity<InfoLevel>,
//! }
//! ```
//!
//! Or implement our [`LogLevel`] trait to customize the default log level and help output.

#![cfg_attr(docsrs, feature(doc_auto_cfg))]
#![warn(clippy::print_stderr)]
#![warn(clippy::print_stdout)]

use std::fmt;

#[cfg(feature = "log")]
pub mod log;
#[cfg(feature = "tracing")]
pub mod tracing;

/// Logging flags to `#[command(flatten)]` into your CLI
#[derive(clap::Args, Debug, Clone, Default)]
#[command(about = None, long_about = None)]
pub struct Verbosity<L: LogLevel = ErrorLevel> {
    #[arg(
        long,
        short = 'v',
        action = clap::ArgAction::Count,
        global = true,
        help = L::verbose_help(),
        long_help = L::verbose_long_help(),
    )]
    verbose: u8,

    #[arg(
        long,
        short = 'q',
        action = clap::ArgAction::Count,
        global = true,
        help = L::quiet_help(),
        long_help = L::quiet_long_help(),
        conflicts_with = "verbose",
    )]
    quiet: u8,

    #[arg(skip)]
    phantom: std::marker::PhantomData<L>,
}

impl<L: LogLevel> Verbosity<L> {
    /// Create a new verbosity instance by explicitly setting the values
    pub fn new(verbose: u8, quiet: u8) -> Self {
        Verbosity {
            verbose,
            quiet,
            phantom: std::marker::PhantomData,
        }
    }

    /// Whether any verbosity flags (either `--verbose` or `--quiet`)
    /// are present on the command line.
    pub fn is_present(&self) -> bool {
        self.verbose != 0 || self.quiet != 0
    }

    /// If the user requested complete silence (i.e. not just no-logging).
    pub fn is_silent(&self) -> bool {
        self.filter() == VerbosityFilter::Off
    }

    /// Gets the filter that should be applied to the logger.
    pub fn filter(&self) -> VerbosityFilter {
        let offset = self.verbose as i16 - self.quiet as i16;
        L::default_filter().with_offset(offset)
    }
}

#[cfg(feature = "log")]
impl<L: LogLevel> Verbosity<L> {
    /// Get the log level.
    ///
    /// `None` means all output is disabled.
    pub fn log_level(&self) -> Option<log::Level> {
        self.filter().into()
    }

    /// Get the log level filter.
    pub fn log_level_filter(&self) -> log::LevelFilter {
        self.filter().into()
    }
}

#[cfg(feature = "tracing")]
impl<L: LogLevel> Verbosity<L> {
    /// Get the tracing level.
    ///
    /// `None` means all output is disabled.
    pub fn tracing_level(&self) -> Option<tracing_core::Level> {
        self.filter().into()
    }

    /// Get the tracing level filter.
    pub fn tracing_level_filter(&self) -> tracing_core::LevelFilter {
        self.filter().into()
    }
}

impl<L: LogLevel> fmt::Display for Verbosity<L> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.filter().fmt(f)
    }
}

/// Customize the default log-level and associated help
pub trait LogLevel {
    /// Baseline level before applying `--verbose` and `--quiet`
    fn default_filter() -> VerbosityFilter;

    /// Short-help message for `--verbose`
    fn verbose_help() -> Option<&'static str> {
        Some("Increase logging verbosity")
    }

    /// Long-help message for `--verbose`
    fn verbose_long_help() -> Option<&'static str> {
        None
    }

    /// Short-help message for `--quiet`
    fn quiet_help() -> Option<&'static str> {
        Some("Decrease logging verbosity")
    }

    /// Long-help message for `--quiet`
    fn quiet_long_help() -> Option<&'static str> {
        None
    }
}

/// A representation of the log level filter.
///
/// Used to calculate the log level and filter.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VerbosityFilter {
    Off,
    Error,
    Warn,
    Info,
    Debug,
    Trace,
}

impl VerbosityFilter {
    /// Apply an offset to the filter level.
    ///
    /// Negative values will decrease the verbosity, while positive values will increase it.
    fn with_offset(&self, offset: i16) -> VerbosityFilter {
        let value = match self {
            Self::Off => 0_i16,
            Self::Error => 1,
            Self::Warn => 2,
            Self::Info => 3,
            Self::Debug => 4,
            Self::Trace => 5,
        };
        match value.saturating_add(offset) {
            i16::MIN..=0 => Self::Off,
            1 => Self::Error,
            2 => Self::Warn,
            3 => Self::Info,
            4 => Self::Debug,
            5..=i16::MAX => Self::Trace,
        }
    }
}

impl fmt::Display for VerbosityFilter {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Off => write!(f, "off"),
            Self::Error => write!(f, "error"),
            Self::Warn => write!(f, "warn"),
            Self::Info => write!(f, "info"),
            Self::Debug => write!(f, "debug"),
            Self::Trace => write!(f, "trace"),
        }
    }
}

/// Default to [`VerbosityFilter::Error`]
#[derive(Copy, Clone, Debug, Default)]
pub struct ErrorLevel;

impl LogLevel for ErrorLevel {
    fn default_filter() -> VerbosityFilter {
        VerbosityFilter::Error
    }
}

/// Default to [`VerbosityFilter::Warn`]
#[derive(Copy, Clone, Debug, Default)]
pub struct WarnLevel;

impl LogLevel for WarnLevel {
    fn default_filter() -> VerbosityFilter {
        VerbosityFilter::Warn
    }
}

/// Default to [`VerbosityFilter::Info`]
#[derive(Copy, Clone, Debug, Default)]
pub struct InfoLevel;

impl LogLevel for InfoLevel {
    fn default_filter() -> VerbosityFilter {
        VerbosityFilter::Info
    }
}

/// Default to [`VerbosityFilter::Debug`]
#[derive(Copy, Clone, Debug, Default)]
pub struct DebugLevel;

impl LogLevel for DebugLevel {
    fn default_filter() -> VerbosityFilter {
        VerbosityFilter::Debug
    }
}

/// Default to [`VerbosityFilter::Trace`]
#[derive(Copy, Clone, Debug, Default)]
pub struct TraceLevel;

impl LogLevel for TraceLevel {
    fn default_filter() -> VerbosityFilter {
        VerbosityFilter::Trace
    }
}

/// Default to [`VerbosityFilter::Off`] (no logging)
#[derive(Copy, Clone, Debug, Default)]
pub struct OffLevel;

impl LogLevel for OffLevel {
    fn default_filter() -> VerbosityFilter {
        VerbosityFilter::Off
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn verify_app() {
        #[derive(Debug, clap::Parser)]
        struct Cli {
            #[command(flatten)]
            verbose: Verbosity,
        }

        use clap::CommandFactory;
        Cli::command().debug_assert();
    }

    /// Asserts that the filter is correct for the given verbosity and quiet values.
    #[track_caller]
    fn assert_filter<L: LogLevel>(verbose: u8, quiet: u8, expected: VerbosityFilter) {
        assert_eq!(
            Verbosity::<L>::new(verbose, quiet).filter(),
            expected,
            "verbose = {verbose}, quiet = {quiet}"
        );
    }

    #[test]
    fn verbosity_off_level() {
        let tests = [
            (0, 0, VerbosityFilter::Off),
            (1, 0, VerbosityFilter::Error),
            (2, 0, VerbosityFilter::Warn),
            (3, 0, VerbosityFilter::Info),
            (4, 0, VerbosityFilter::Debug),
            (5, 0, VerbosityFilter::Trace),
            (6, 0, VerbosityFilter::Trace),
            (255, 0, VerbosityFilter::Trace),
            (0, 1, VerbosityFilter::Off),
            (0, 255, VerbosityFilter::Off),
            (255, 255, VerbosityFilter::Off),
        ];

        for (verbose, quiet, expected_filter) in tests {
            assert_filter::<OffLevel>(verbose, quiet, expected_filter);
        }
    }

    #[test]
    fn verbosity_error_level() {
        let tests = [
            (0, 0, VerbosityFilter::Error),
            (1, 0, VerbosityFilter::Warn),
            (2, 0, VerbosityFilter::Info),
            (3, 0, VerbosityFilter::Debug),
            (4, 0, VerbosityFilter::Trace),
            (5, 0, VerbosityFilter::Trace),
            (255, 0, VerbosityFilter::Trace),
            (0, 1, VerbosityFilter::Off),
            (0, 2, VerbosityFilter::Off),
            (0, 255, VerbosityFilter::Off),
            (255, 255, VerbosityFilter::Error),
        ];

        for (verbose, quiet, expected_filter) in tests {
            assert_filter::<ErrorLevel>(verbose, quiet, expected_filter);
        }
    }

    #[test]
    fn verbosity_warn_level() {
        let tests = [
            // verbose, quiet, expected_level, expected_filter
            (0, 0, VerbosityFilter::Warn),
            (1, 0, VerbosityFilter::Info),
            (2, 0, VerbosityFilter::Debug),
            (3, 0, VerbosityFilter::Trace),
            (4, 0, VerbosityFilter::Trace),
            (255, 0, VerbosityFilter::Trace),
            (0, 1, VerbosityFilter::Error),
            (0, 2, VerbosityFilter::Off),
            (0, 3, VerbosityFilter::Off),
            (0, 255, VerbosityFilter::Off),
            (255, 255, VerbosityFilter::Warn),
        ];

        for (verbose, quiet, expected_filter) in tests {
            assert_filter::<WarnLevel>(verbose, quiet, expected_filter);
        }
    }

    #[test]
    fn verbosity_info_level() {
        let tests = [
            // verbose, quiet, expected_level, expected_filter
            (0, 0, VerbosityFilter::Info),
            (1, 0, VerbosityFilter::Debug),
            (2, 0, VerbosityFilter::Trace),
            (3, 0, VerbosityFilter::Trace),
            (255, 0, VerbosityFilter::Trace),
            (0, 1, VerbosityFilter::Warn),
            (0, 2, VerbosityFilter::Error),
            (0, 3, VerbosityFilter::Off),
            (0, 4, VerbosityFilter::Off),
            (0, 255, VerbosityFilter::Off),
            (255, 255, VerbosityFilter::Info),
        ];

        for (verbose, quiet, expected_filter) in tests {
            assert_filter::<InfoLevel>(verbose, quiet, expected_filter);
        }
    }

    #[test]
    fn verbosity_debug_level() {
        let tests = [
            // verbose, quiet, expected_level, expected_filter
            (0, 0, VerbosityFilter::Debug),
            (1, 0, VerbosityFilter::Trace),
            (2, 0, VerbosityFilter::Trace),
            (255, 0, VerbosityFilter::Trace),
            (0, 1, VerbosityFilter::Info),
            (0, 2, VerbosityFilter::Warn),
            (0, 3, VerbosityFilter::Error),
            (0, 4, VerbosityFilter::Off),
            (0, 5, VerbosityFilter::Off),
            (0, 255, VerbosityFilter::Off),
            (255, 255, VerbosityFilter::Debug),
        ];

        for (verbose, quiet, expected_filter) in tests {
            assert_filter::<DebugLevel>(verbose, quiet, expected_filter);
        }
    }

    #[test]
    fn verbosity_trace_level() {
        let tests = [
            // verbose, quiet, expected_level, expected_filter
            (0, 0, VerbosityFilter::Trace),
            (1, 0, VerbosityFilter::Trace),
            (255, 0, VerbosityFilter::Trace),
            (0, 1, VerbosityFilter::Debug),
            (0, 2, VerbosityFilter::Info),
            (0, 3, VerbosityFilter::Warn),
            (0, 4, VerbosityFilter::Error),
            (0, 5, VerbosityFilter::Off),
            (0, 6, VerbosityFilter::Off),
            (0, 255, VerbosityFilter::Off),
            (255, 255, VerbosityFilter::Trace),
        ];

        for (verbose, quiet, expected_filter) in tests {
            assert_filter::<TraceLevel>(verbose, quiet, expected_filter);
        }
    }
}
