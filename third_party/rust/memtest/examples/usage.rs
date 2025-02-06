use {
    anyhow::Context,
    memtest::{MemtestKind, MemtestRunner, MemtestRunnerArgs},
    rand::{seq::SliceRandom, thread_rng},
    std::{
        mem::size_of,
        time::{Duration, Instant},
    },
    tracing::info,
    tracing_subscriber::fmt::format::FmtSpan,
};

// TODO: Command line option for json output
fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_span_events(FmtSpan::NEW | FmtSpan::CLOSE)
        .with_max_level(tracing::Level::TRACE)
        .with_writer(std::io::stderr)
        .with_thread_ids(true)
        .init();
    let start_time = Instant::now();
    let (mem_usize_count, memtest_runner_args, memtest_kinds) = match parse_args() {
        Ok(parsed_args) => parsed_args,
        Err(s) => {
            eprintln!(concat!(
                "Usage: memtest-runner ",
                "<memsize in MiB> ",
                "<timeout in ms> ",
                "<mem_lock_mode> ",
                "<allow_working_set_resize as bool> ",
                "<allow_multithread as bool> ",
                "<allow_early_temrmination as bool> ",
                "<memtest_kinds as space separated string>"
            ));
            anyhow::bail!("Invalid/missing argument '{s}'");
        }
    };

    info!("Running memtest-runner with: {memtest_runner_args:#?}");
    let mut memory = vec![0; mem_usize_count];
    let report_list = MemtestRunner::from_test_kinds(&memtest_runner_args, memtest_kinds)
        .run(&mut memory)
        .context("Failed to run memtest-runner")?;
    println!("Tester ran for {:?}", start_time.elapsed());
    println!("Test results: \n{report_list}");

    anyhow::ensure!(
        report_list.all_pass(),
        "Found failures or errors among memtest reports"
    );
    Ok(())
}

/// Parse command line arguments to return a usize for the requested memory vector length and
/// other MemtestRunner arguments
fn parse_args() -> Result<(usize, MemtestRunnerArgs, Vec<MemtestKind>), &'static str> {
    const KIB: usize = 1024;
    const MIB: usize = 1024 * KIB;

    let mut iter = std::env::args().skip(1);

    macro_rules! parse_next(($n: literal) => {
        iter.next().and_then(|s| s.parse().ok()).ok_or($n)?
    });

    let memsize: usize = parse_next!("memsize");
    let mem_usize_count = memsize * MIB / size_of::<usize>();

    let memtest_runner_args = MemtestRunnerArgs {
        timeout: Duration::from_millis(parse_next!("timeout_ms")),
        mem_lock_mode: parse_next!("mem_lock_mode"),
        allow_working_set_resize: parse_next!("allow_working_set_resize"),
        allow_multithread: parse_next!("allow_multithread"),
        allow_early_termination: parse_next!("allow_early_termination"),
    };

    let memtest_kinds_string: String = parse_next!("memtest_kinds");
    let memtest_kinds = memtest_kinds_from_str(&memtest_kinds_string)?;

    Ok((mem_usize_count, memtest_runner_args, memtest_kinds))
}

/// Returns a vector of MemtestKind that contains all kinds, but prioritizes the given memtests.
fn memtest_kinds_from_str(str: &str) -> Result<Vec<MemtestKind>, &'static str> {
    let specified = str
        .split_whitespace()
        .map(|s| s.parse().map_err(|_| "memtest_kinds"))
        .collect::<Result<Vec<MemtestKind>, &'static str>>()?;

    let mut remaining: Vec<_> = MemtestKind::ALL
        .iter()
        .filter(|k| !specified.contains(k))
        .cloned()
        .collect();
    remaining.shuffle(&mut thread_rng());

    Ok([specified, remaining].concat())
}
