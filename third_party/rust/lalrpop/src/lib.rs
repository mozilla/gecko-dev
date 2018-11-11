// Need this for rusty_peg
#![recursion_limit = "256"]
// I hate this lint.
#![allow(unused_parens)]
// The builtin tests don't cover the CLI and so forth, and it's just
// too darn annoying to try and make them do so.
#![cfg_attr(test, allow(dead_code))]

extern crate ascii_canvas;
extern crate atty;
extern crate bit_set;
extern crate diff;
extern crate ena;
extern crate itertools;
extern crate lalrpop_util;
extern crate petgraph;
extern crate regex;
extern crate regex_syntax;
extern crate string_cache;
extern crate term;
extern crate unicode_xid;
extern crate sha2;

#[cfg(test)]
extern crate rand;

// hoist the modules that define macros up earlier
#[macro_use]
mod rust;
#[macro_use]
mod log;

mod api;
mod build;
mod collections;
mod file_text;
mod grammar;
mod lexer;
mod lr1;
mod message;
mod normalize;
mod parser;
mod kernel_set;
mod session;
mod tls;
mod tok;
mod util;

#[cfg(test)]
mod generate;
#[cfg(test)]
mod test_util;

pub use api::Configuration;
pub use api::process_root;
pub use api::process_root_unconditionally;
use ascii_canvas::style;
