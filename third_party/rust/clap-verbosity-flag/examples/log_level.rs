//! Demonstrates how to set the default log level for the logger to something other than the default
//! (`ErrorLevel`). This is done with multiple subcommands, each with their own verbosity level.

use clap::{Parser, Subcommand};
use clap_verbosity_flag::{
    DebugLevel, ErrorLevel, InfoLevel, OffLevel, TraceLevel, Verbosity, WarnLevel,
};

#[derive(Debug, Parser)]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    Off {
        #[command(flatten)]
        verbose: Verbosity<OffLevel>,
    },
    Error {
        #[command(flatten)]
        verbose: Verbosity<ErrorLevel>,
    },
    Warn {
        #[command(flatten)]
        verbose: Verbosity<WarnLevel>,
    },
    Info {
        #[command(flatten)]
        verbose: Verbosity<InfoLevel>,
    },
    Debug {
        #[command(flatten)]
        verbose: Verbosity<DebugLevel>,
    },
    Trace {
        #[command(flatten)]
        verbose: Verbosity<TraceLevel>,
    },
}

impl Command {
    fn log_level_filter(&self) -> log::LevelFilter {
        match self {
            Command::Off { verbose } => verbose.log_level_filter(),
            Command::Error { verbose } => verbose.log_level_filter(),
            Command::Warn { verbose } => verbose.log_level_filter(),
            Command::Info { verbose } => verbose.log_level_filter(),
            Command::Debug { verbose } => verbose.log_level_filter(),
            Command::Trace { verbose } => verbose.log_level_filter(),
        }
    }
}

fn main() {
    let cli = Cli::parse();
    env_logger::Builder::new()
        .filter_level(cli.command.log_level_filter())
        .init();

    log::error!("Engines exploded");
    log::warn!("Engines smoking");
    log::info!("Engines exist");
    log::debug!("Engine temperature is 200 degrees");
    log::trace!("Engine subsection is 300 degrees");
}
