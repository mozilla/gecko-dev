// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::unwrap_used, reason = "OK in a build script.")]

use std::env;

fn main() {
    let target = env::var("TARGET").unwrap();
    if target.contains("windows") {
        println!("cargo:rustc-link-lib=winmm");
    }

    // `cfg(sanitize = "..")` is not stabilized.
    //
    // See <https://doc.rust-lang.org/stable/unstable-book/language-features/cfg-sanitize.html>.
    println!("cargo:rustc-check-cfg=cfg(neqo_sanitize)");
    if env::var("CARGO_CFG_SANITIZE").is_ok() {
        println!("cargo:rustc-cfg=neqo_sanitize");
    }
}
