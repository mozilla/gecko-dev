/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A frontend for memory testing.

use {
    crate::std::{env, mem::size_of},
    anyhow::Context,
    memtest::{MemtestKind, MemtestRunner, MemtestRunnerArgs},
    rand::{seq::SliceRandom, thread_rng},
    serde_json,
};

// runtimeobject is not automatically linked correctly
#[cfg(windows)]
#[link(name = "runtimeobject")]
extern "C" {}

/// Usage: crashreporter --memtest <memsize_mb> <json formatted memtest_runner_args>
///        Set the env var `MOZ_CRASHREPORTER_MEMTEST_KINDS` to specify prioritized memtest kinds
pub fn main() {
    let (mem_usize_count, memtest_runner_args, memtest_kinds) = match parse_args() {
        Ok(parsed) => parsed,
        Err(e) => {
            eprintln!("Error: {e:?}");
            std::process::exit(1);
        }
    };
    let mut memory = vec![0; mem_usize_count];

    let memtest_runner_result = MemtestRunner::from_test_kinds(&memtest_runner_args, memtest_kinds)
        .run(&mut memory)
        .expect("failed to run memtest-runner");
    println!(
        "{}",
        serde_json::to_string(&memtest_runner_result)
            .expect("memtest runner results failed to serialize")
    );
}

/// Parse command line arguments and environment to return the parameters for running memtest,
/// including a usize for the requested memory vector length, MemtestRunnerArgs and a vector of
/// MemtestKinds to run.
fn parse_args() -> anyhow::Result<(usize, MemtestRunnerArgs, Vec<MemtestKind>)> {
    const KIB: usize = 1024;
    const MIB: usize = 1024 * KIB;

    let mut iter = env::args_os().skip(2).map_while(|s| s.into_string().ok());

    let memsize_mb: usize = iter
        .next()
        .and_then(|s| s.parse().ok())
        .context("missing/invalid memsize_MB")?;
    let memtest_runner_args = iter
        .next()
        .and_then(|s| serde_json::from_str(&s).ok())
        .context("missing/invalid memtest_runner_args")?;

    Ok((
        memsize_mb * MIB / size_of::<usize>(),
        memtest_runner_args,
        get_memtest_kinds()?,
    ))
}

/// Returns a vector of MemtestKind that contains all kinds, but prioritizes the given kinds.
fn get_memtest_kinds() -> anyhow::Result<Vec<MemtestKind>> {
    let env_string = env::var(ekey!("MEMTEST_KINDS")).unwrap_or_default();

    let specified = env_string
        .split_whitespace()
        .map(|s| s.parse().context("failed to parse memtest kinds"))
        .collect::<anyhow::Result<Vec<MemtestKind>>>()?;

    let mut remaining: Vec<_> = MemtestKind::ALL
        .iter()
        .filter(|k| !specified.contains(k))
        .cloned()
        .collect();
    remaining.shuffle(&mut thread_rng());

    Ok([specified, remaining].concat())
}
