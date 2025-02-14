// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_common::{qdebug, qerror, qinfo, qtrace, qwarn};

#[test]
fn basic() {
    qerror!("error");
    qwarn!("warn");
    qinfo!("info");
    qdebug!("debug");
    qtrace!("trace");
}

#[test]
fn args() {
    let num = 1;
    let obj = std::time::Instant::now();
    qerror!("error {num} {obj:?}");
    qwarn!("warn {num} {obj:?}");
    qinfo!("info {num} {obj:?}");
    qdebug!("debug {num} {obj:?}");
    qtrace!("trace {num} {obj:?}");
}
