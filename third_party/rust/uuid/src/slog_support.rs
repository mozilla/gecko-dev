// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use slog;
use Uuid;

impl slog::Value for Uuid {
    fn serialize(
        &self,
        _: &slog::Record,
        key: slog::Key,
        serializer: &mut slog::Serializer,
    ) -> Result<(), slog::Error> {
        serializer.emit_arguments(key, &format_args!("{}", self))
    }
}

#[cfg(test)]
mod tests {

    #[test]
    fn test_slog_kv() {
        use slog;
        use test_util;
        use slog::Drain;

        let root = slog::Logger::root(slog::Discard.fuse(), o!());
        let u1 = test_util::new();
        crit!(root, "test"; "u1" => u1);
    }
}