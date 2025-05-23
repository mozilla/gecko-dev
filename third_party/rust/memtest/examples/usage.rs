use {
    anyhow::Context,
    memtest::{Runner, RunnerArgs, TestKind},
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
    let (mem_usize_count, runner_args, test_kinds) = match parse_args() {
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
                "<test_kinds as space separated string>"
            ));
            anyhow::bail!("Invalid/missing argument '{s}'");
        }
    };

    info!("Running memtest-runner with: {runner_args:#?}");
    let mut memory = vec![0; mem_usize_count];
    let report_list = Runner::from_test_kinds(&runner_args, test_kinds)
        .run(&mut memory)
        .context("Failed to run memtest-runner")?;
    println!("Tester ran for {:?}", start_time.elapsed());
    println!("Test results: \n{report_list}");

    anyhow::ensure!(
        report_list.all_pass(),
        "Found failures or errors among test reports"
    );
    Ok(())
}

/// Parse command line arguments to return a usize for the requested memory vector length and
/// other Runner arguments
fn parse_args() -> Result<(usize, RunnerArgs, Vec<TestKind>), &'static str> {
    const KIB: usize = 1024;
    const MIB: usize = 1024 * KIB;

    let mut iter = std::env::args().skip(1);

    macro_rules! parse_next(($n: literal) => {
        iter.next().and_then(|s| s.parse().ok()).ok_or($n)?
    });

    let memsize: usize = parse_next!("memsize");
    let mem_usize_count = memsize * MIB / size_of::<usize>();

    let runner_args = RunnerArgs {
        timeout: Duration::from_millis(parse_next!("timeout_ms")),
        mem_lock_mode: parse_next!("mem_lock_mode"),
        allow_working_set_resize: parse_next!("allow_working_set_resize"),
        allow_multithread: parse_next!("allow_multithread"),
        allow_early_termination: parse_next!("allow_early_termination"),
    };

    let test_kinds_string: String = parse_next!("test_kinds");
    let test_kinds = test_kinds_from_str(&test_kinds_string)?;

    Ok((mem_usize_count, runner_args, test_kinds))
}

/// Returns a vector of TestKind that contains all kinds, but prioritizes the given test kinds.
fn test_kinds_from_str(str: &str) -> Result<Vec<TestKind>, &'static str> {
    let specified = str
        .split_whitespace()
        .map(|s| s.parse().map_err(|_| "test_kinds"))
        .collect::<Result<Vec<TestKind>, &'static str>>()?;

    let mut remaining: Vec<_> = TestKind::ALL
        .iter()
        .filter(|k| !specified.contains(k))
        .cloned()
        .collect();
    remaining.shuffle(&mut thread_rng());

    Ok([specified, remaining].concat())
}
