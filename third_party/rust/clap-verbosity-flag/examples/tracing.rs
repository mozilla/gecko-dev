use clap::Parser;
use clap_verbosity_flag::Verbosity;

/// Foo
#[derive(Debug, Parser)]
struct Cli {
    #[command(flatten)]
    verbosity: Verbosity,
}

fn main() {
    let cli = Cli::parse();

    tracing_subscriber::fmt()
        .with_max_level(cli.verbosity)
        .init();

    tracing::error!("Engines exploded");
    tracing::warn!("Engines smoking");
    tracing::info!("Engines exist");
    tracing::debug!("Engine temperature is 200 degrees");
    tracing::trace!("Engine subsection is 300 degrees");
}
