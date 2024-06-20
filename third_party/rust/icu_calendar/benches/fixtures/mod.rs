// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

pub mod structs;

use std::fs::File;
use std::io::BufReader;

#[allow(dead_code)]
pub fn get_dates_fixture() -> std::io::Result<structs::DateFixture> {
    let file = File::open("./benches/fixtures/datetimes.json")?;
    let reader = BufReader::new(file);

    Ok(serde_json::from_reader(reader)?)
}
