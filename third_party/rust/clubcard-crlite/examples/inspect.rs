/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use clubcard_crlite::CRLiteClubcard;
use std::env::args;
use std::path::PathBuf;
use std::process::ExitCode;

fn parse_args() -> Option<PathBuf> {
    let mut args = args().map(PathBuf::from);
    let _name = args.next()?;
    Some(args.next()?)
}

fn main() -> std::process::ExitCode {
    let Some(filter_path) = parse_args() else {
        eprintln!("Usage: {} <filter>", args().next().unwrap());
        return ExitCode::FAILURE;
    };

    let Ok(filter_bytes) = std::fs::read(&filter_path) else {
        eprintln!("Could not read filter");
        return ExitCode::FAILURE;
    };

    let Ok(filter) = CRLiteClubcard::from_bytes(&filter_bytes) else {
        eprintln!("Could not parse filter");
        return ExitCode::FAILURE;
    };

    println!("{}", filter);

    ExitCode::SUCCESS
}
