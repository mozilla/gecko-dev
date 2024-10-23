/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A frontend for minidump analysis.

use minidump_analyzer::MinidumpAnalyzer;

pub fn main() {
    // Skip program name and `--analyze` argument.
    let mut args = std::env::args_os().skip(2);

    let mut minidump_path = None;
    let mut analyze_all_threads = false;
    while let Some(arg) = args.next() {
        if arg == "--full" && !analyze_all_threads {
            analyze_all_threads = true;
        } else if minidump_path.is_none() {
            minidump_path = Some(arg);
        } else {
            eprintln!("ignoring extraneous argument: {}", arg.to_string_lossy());
        }
    }

    let Some(minidump_path) = minidump_path else {
        eprintln!("expected minidump path to analyze");
        std::process::exit(1);
    };

    if let Err(e) = MinidumpAnalyzer::new(minidump_path.as_ref())
        .all_threads(analyze_all_threads)
        .analyze()
    {
        eprintln!("minidump analyzer failed: {e}");
        std::process::exit(1);
    }
}
