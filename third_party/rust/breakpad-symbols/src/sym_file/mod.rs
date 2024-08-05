// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.
use crate::{FrameSymbolizer, FrameWalker, Module, SymbolError};

pub use crate::sym_file::types::*;
pub use parser::SymbolParser;
use std::fs::File;
use std::io::Read;
use std::ops::Deref;
use std::path::Path;
use tracing::trace;

mod parser;
mod types;
pub mod walker;

// # Sync VS Async
//
// There is both a sync and an async entry-point to the parser.
// The two impls should be essentially identical, except for how they
// read bytes from the input reader into our circular buffer.
//
//
// # Streaming
//
// This parser streams the input to avoid the need to materialize all of
// it into memory at once (symbol files can be a gigabyte!). As a result,
// we need to iteratively parse.
//
// We do this by repeatedly filling up a buffer with input and asking the
// parser to parse it. The parser will return how much of the input it
// consumed, which we can use to clear space in our buffer and to tell
// if it successfully consumed the whole input when the Reader runs dry.
//
//
// # Handling EOF / Capacity
//
// Having a fix-sized buffer has one fatal issue: if one atomic step
// of the parser needs more than this amount of data, then we won't
// be able to parse it.
//
// This can result in `buf` filling up and `buf.space()` becoming an
// empty slice. This in turn will make the reader yield 0 bytes, and
// we'll treat it like EOF and fail the parse. When this happens, we
// try to double the buffer's size and request more bytes. If we get
// more, hooray! If we don't, then it's a "real" EOF.
//
// The "atom" of our parser is a line, so we need our buffer to be able
// to fit any line. However we actually only have roughly
// *half* this value as our limit, as circular::Buffer will only
// `shift` the buffer's contents if over half of its capacity has been
// drained by `consume` -- and `space()` only grows when a `shift` happens.
//
// I have in fact seen 8kb function names from Rust (thanks generic combinators!)
// and 82kb function names from C++ (thanks 'auto' returns!), so we
// need a buffer size that can grow to at least 200KB. This is a *very* large
// amount to backshift repeatedly, so to keep this under control, we start
// with only a 10KB buffer, which is generous but tolerable.
//
// We should still have *SOME* limit on this to avoid nasty death spirals,
// so let's go with 2MB (MAX_BUFFER_CAPACITY), letting you have a horrifying 1MB symbol.
//
// But just *dying* when we hit this point is terrible, so lets have an
// extra layer of robustness: if we ever hit the limit, enter "panic recovery"
// and just start discarding bytes until we hit a newline. Then resume normal
// parsing. The net effect of this is that we just treat this one line as
// corrupt (because statistically it won't even be needed!).

// Allows for at least 80KB symbol names, at most 160KB symbol names (fuzzy because of circular).
static MAX_BUFFER_CAPACITY: usize = 1024 * 160;
static INITIAL_BUFFER_CAPACITY: usize = 1024 * 10;

impl SymbolFile {
    /// Parse a SymbolFile from the given Reader.
    ///
    /// Every time a chunk of the input is parsed, that chunk will
    /// be passed to `callback` to allow you to do something else
    /// with the data as it's streamed in (e.g. you can save the
    /// input to a cache).
    ///
    /// The reader is wrapped in a buffer reader so you shouldn't
    /// buffer the input yourself.
    pub fn parse<R: Read>(
        mut input_reader: R,
        mut callback: impl FnMut(&[u8]),
    ) -> Result<SymbolFile, SymbolError> {
        let mut buf = circular::Buffer::with_capacity(INITIAL_BUFFER_CAPACITY);
        let mut parser = SymbolParser::new();
        let mut fully_consumed = false;
        let mut tried_to_grow = false;
        let mut in_panic_recovery = false;
        let mut just_finished_recovering = false;
        let mut total_consumed = 0u64;
        loop {
            if in_panic_recovery {
                // PANIC RECOVERY MODE! DISCARD BYTES UNTIL NEWLINE.
                let input = buf.data();
                if let Some(new_line_idx) = input.iter().position(|&byte| byte == b'\n') {
                    // Hooray, we found a new line! Consume up to and including that, and resume.
                    let amount = new_line_idx + 1;
                    callback(&input[..amount]);
                    buf.consume(amount);
                    total_consumed += amount as u64;

                    // Back to normal!
                    in_panic_recovery = false;
                    fully_consumed = false;
                    just_finished_recovering = true;
                    parser.lines += 1;
                    trace!("RECOVERY: complete!");
                } else {
                    // No newline, discard everything
                    let amount = input.len();
                    callback(&input[..amount]);
                    buf.consume(amount);
                    total_consumed += amount as u64;

                    // If the next read returns 0 bytes, then that's a proper EOF!
                    fully_consumed = true;
                }
            }

            // Read the data in, and tell the circular buffer about the new data
            let size = input_reader.read(buf.space())?;
            buf.fill(size);

            if size == 0 {
                // If the reader returned no more bytes, this can be either mean
                // EOF or the buffer is out of capacity. There are a lot of cases
                // to consider, so let's go through them one at a time...
                if just_finished_recovering && !buf.data().is_empty() {
                    // We just finished PANIC RECOVERY, but there's still bytes in
                    // the buffer. Assume that is parseable and resume normal parsing
                    // (do nothing, fallthrough to normal path).
                } else if fully_consumed {
                    // Success! The last iteration cleared the buffer and we still got
                    // no more bytes, so that's a proper EOF with a complete parse!
                    return Ok(parser.finish());
                } else if !tried_to_grow {
                    // We still have some stuff in the buffer, assume this is because
                    // the buffer is full, and try to make it BIGGER and ask for more again.
                    let new_cap = buf.capacity().saturating_mul(2);
                    if new_cap > MAX_BUFFER_CAPACITY {
                        // TIME TO PANIC!!! This line is catastrophically big, just start
                        // discarding bytes until we hit a newline.
                        trace!("RECOVERY: discarding enormous line {}", parser.lines);
                        in_panic_recovery = true;
                        continue;
                    }
                    trace!("parser out of space? trying more ({}KB)", new_cap / 1024);
                    buf.grow(new_cap);
                    tried_to_grow = true;
                    continue;
                } else if total_consumed == 0 {
                    // We grew the buffer and still got no more bytes, so it's a proper EOF.
                    // But actually, we never consumed any bytes, so this is an empty file?
                    // Give a better error message for that.
                    return Err(SymbolError::ParseError(
                        "empty SymbolFile (probably something wrong with your debuginfo tooling?)",
                        0,
                    ));
                } else {
                    // Ok give up, this input is just impossible.
                    return Err(SymbolError::ParseError(
                        "unexpected EOF during parsing of SymbolFile (or a line was too long?)",
                        parser.lines,
                    ));
                }
            } else {
                tried_to_grow = false;
            }

            if in_panic_recovery {
                // Don't run the normal parser while we're still recovering!
                continue;
            }
            just_finished_recovering = false;

            // Ask the parser to parse more of the input
            let input = buf.data();
            let consumed = parser.parse_more(input)?;
            total_consumed += consumed as u64;

            // Give the other consumer of this Reader a chance to use this data.
            callback(&input[..consumed]);

            // Remember for the next iteration if all the input was consumed.
            fully_consumed = input.len() == consumed;
            buf.consume(consumed);
        }
    }

    /// `parse` but async
    #[cfg(feature = "http")]
    pub async fn parse_async(
        mut response: reqwest::Response,
        mut callback: impl FnMut(&[u8]),
    ) -> Result<SymbolFile, SymbolError> {
        let mut chunk;
        let mut slice = &[][..];
        let mut input_reader = &mut slice;
        let mut buf = circular::Buffer::with_capacity(INITIAL_BUFFER_CAPACITY);
        let mut parser = SymbolParser::new();

        let mut fully_consumed = false;
        let mut tried_to_grow = false;
        let mut in_panic_recovery = false;
        let mut just_finished_recovering = false;
        let mut total_consumed = 0u64;
        loop {
            if in_panic_recovery {
                // PANIC RECOVERY MODE! DISCARD BYTES UNTIL NEWLINE.
                let input = buf.data();
                if let Some(new_line_idx) = input.iter().position(|&byte| byte == b'\n') {
                    // Hooray, we found a new line! Consume up to and including that, and resume.
                    let amount = new_line_idx + 1;
                    callback(&input[..amount]);
                    buf.consume(amount);
                    total_consumed += amount as u64;

                    // Back to normal!
                    in_panic_recovery = false;
                    fully_consumed = false;
                    just_finished_recovering = true;
                    parser.lines += 1;
                    trace!("PANIC RECOVERY: complete!");
                } else {
                    // No newline, discard everything
                    let amount = input.len();
                    callback(&input[..amount]);
                    buf.consume(amount);
                    total_consumed += amount as u64;

                    // If the next read returns 0 bytes, then that's a proper EOF!
                    fully_consumed = true;
                }
            }

            // Little rube-goldberg machine to stream the contents:
            // * get a chunk (Bytes) from the Response
            // * get its underlying slice
            // * then get a mutable reference to that slice
            // * then Read that mutable reference in our circular buffer
            // * when the slice runs out, get the next chunk and repeat
            if input_reader.is_empty() {
                chunk = response
                    .chunk()
                    .await
                    .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?
                    .unwrap_or_default();
                slice = &chunk[..];
                input_reader = &mut slice;
            }

            // Read the data in, and tell the circular buffer about the new data
            let size = input_reader.read(buf.space())?;
            buf.fill(size);

            if size == 0 {
                // If the reader returned no more bytes, this can be either mean
                // EOF or the buffer is out of capacity. There are a lot of cases
                // to consider, so let's go through them one at a time...
                if just_finished_recovering && !buf.data().is_empty() {
                    // We just finished PANIC RECOVERY, but there's still bytes in
                    // the buffer. Assume that is parseable and resume normal parsing
                    // (do nothing, fallthrough to normal path).
                } else if fully_consumed {
                    // Success! The last iteration cleared the buffer and we still got
                    // no more bytes, so that's a proper EOF with a complete parse!
                    return Ok(parser.finish());
                } else if !tried_to_grow {
                    // We still have some stuff in the buffer, assume this is because
                    // the buffer is full, and try to make it BIGGER and ask for more again.
                    let new_cap = buf.capacity().saturating_mul(2);
                    if new_cap > MAX_BUFFER_CAPACITY {
                        // TIME TO PANIC!!! This line is catastrophically big, just start
                        // discarding bytes until we hit a newline.
                        trace!("RECOVERY: discarding enormous line {}", parser.lines);
                        in_panic_recovery = true;
                        continue;
                    }
                    trace!("parser out of space? trying more ({}KB)", new_cap / 1024);
                    buf.grow(new_cap);
                    tried_to_grow = true;
                    continue;
                } else if total_consumed == 0 {
                    // We grew the buffer and still got no more bytes, so it's a proper EOF.
                    // But actually, we never consumed any bytes, so this is an empty file?
                    // Give a better error message for that.
                    return Err(SymbolError::ParseError(
                        "empty SymbolFile (probably something wrong with your debuginfo tooling?)",
                        0,
                    ));
                } else {
                    // Ok give up, this input is just impossible.
                    return Err(SymbolError::ParseError(
                        "unexpected EOF during parsing of SymbolFile (or a line was too long?)",
                        parser.lines,
                    ));
                }
            } else {
                tried_to_grow = false;
            }

            if in_panic_recovery {
                // Don't run the normal parser while we're still recovering!
                continue;
            }
            just_finished_recovering = false;

            // Ask the parser to parse more of the input
            let input = buf.data();
            let consumed = parser.parse_more(input)?;
            total_consumed += consumed as u64;

            // Give the other consumer of this Reader a chance to use this data.
            callback(&input[..consumed]);

            // Remember for the next iteration if all the input was consumed.
            fully_consumed = input.len() == consumed;
            buf.consume(consumed);
        }
    }

    // Parse a SymbolFile from bytes.
    pub fn from_bytes(bytes: &[u8]) -> Result<SymbolFile, SymbolError> {
        Self::parse(bytes, |_| ())
    }

    // Parse a SymbolFile from a file.
    pub fn from_file(path: &Path) -> Result<SymbolFile, SymbolError> {
        let file = File::open(path)?;
        Self::parse(file, |_| ())
    }

    /// Fill in as much source information for `frame` as possible.
    pub fn fill_symbol(&self, module: &dyn Module, frame: &mut dyn FrameSymbolizer) {
        // Look for a FUNC covering the address first.
        if frame.get_instruction() < module.base_address() {
            return;
        }
        let addr = frame.get_instruction() - module.base_address();
        if let Some(func) = self.functions.get(addr) {
            // TODO: although FUNC records have a parameter size, it appears that
            // they aren't to be trusted? The STACK WIN records are more reliable
            // when available. This is important precisely because these values
            // are used to unwind subsequent STACK WIN frames (because certain
            // calling conventions have the caller push the callee's arguments,
            // which affects the the stack's size!).
            //
            // Need to spend more time thinking about if this is the right approach
            let parameter_size = if let Some(info) = self.win_stack_framedata_info.get(addr) {
                info.parameter_size
            } else if let Some(info) = self.win_stack_fpo_info.get(addr) {
                info.parameter_size
            } else {
                func.parameter_size
            };

            frame.set_function(
                &func.name,
                func.address + module.base_address(),
                parameter_size,
            );

            // See if there's source line and inline info as well.
            //
            // In the following, we transform data between two different representations of inline calls.
            // The input shape has function names associated with the location of the call to that function.
            // The output shape has function names associated with a location *inside* that function.
            //
            // Input:
            //
            //   (
            //       outer_name,
            //       inline_calls: [ // Each location is the line of the *call* to the function
            //           (inline_call_location[0], inline_name[0]),
            //           (inline_call_location[1], inline_name[1]),
            //           (inline_call_location[2], inline_name[2]),
            //       ]
            //       innermost_location,
            //   )
            //
            // Output:
            //
            //   ( // Each location is the line *inside* the function
            //       (outer_name, inline_call_location[0]),
            //       inlines: [
            //           (inline_name[0], inline_call_location[1]),
            //           (inline_name[1], inline_call_location[2]),
            //           (inline_name[2], innermost_location),
            //       ]
            //   )
            if let Some((file_id, line, address, next_inline_origin)) =
                func.get_outermost_sourceloc(addr)
            {
                if let Some(file) = self.files.get(&file_id) {
                    frame.set_source_file(file, line, address + module.base_address());
                }

                if let Some(mut inline_origin) = next_inline_origin {
                    // There is an inline call at the address.
                    // Enumerate all inlines at the address one by one by looking up
                    // successively deeper call depths.
                    // The call to `get_outermost_source_location` above looked up depth 0, so here
                    // we start at depth 1.
                    for depth in 1.. {
                        match func.get_inlinee_at_depth(depth, addr) {
                            Some((call_file_id, call_line, _address, next_inline_origin)) => {
                                // We found another inline frame.
                                let call_file = self.files.get(&call_file_id).map(Deref::deref);
                                if let Some(name) = self.inline_origins.get(&inline_origin) {
                                    frame.add_inline_frame(name, call_file, Some(call_line));
                                }

                                inline_origin = next_inline_origin;
                            }
                            None => break,
                        }
                    }
                    // We've run out of inline calls but we still have to output the final frame.
                    let (file, line) = match func.get_innermost_sourceloc(addr) {
                        Some((file_id, line, _)) => (
                            self.files.get(&file_id).map(Deref::deref),
                            if line != 0 { Some(line) } else { None },
                        ),
                        None => (None, None),
                    };
                    if let Some(name) = self.inline_origins.get(&inline_origin) {
                        frame.add_inline_frame(name, file, line);
                    }
                }
            }
        } else if let Some(public) = self.find_nearest_public(addr) {
            // We couldn't find a valid FUNC record, but we could find a PUBLIC record.
            // Unfortauntely, PUBLIC records don't have end-points, so this could be
            // a random PUBLIC record from the start of the module that isn't at all
            // applicable. To try limit this problem, we can use the nearest FUNC
            // record that comes *before* the address we're trying to find a symbol for.
            //
            // It is reasonable to assume a PUBLIC record cannot extend *past* a FUNC,
            // so if the PUBLIC has a smaller base address than the nearest previous FUNC
            // to our target address, the PUBLIC must actually end before that FUNC and
            // therefore not actually apply to the target address.
            //
            // We get the nearest previous FUNC by getting the raw slice of ranges
            // and binary searching for our base address. Rust's builtin binary search
            // will fail to find the value since it uses strict equality *but* the Err
            // will helpfully contain the index in the slice where our value "should"
            // be inserted to preserve the sort. The element before this index is
            // therefore the nearest previous value!
            //
            // Case analysis for this -1 because binary search is an off-by-one minefield:
            //
            // * if the address we were looking for came *before* every FUNC, binary_search
            //   would yield "0" because that's where it should go to preserve the sort.
            //   The checked_sub will then fail and make us just assume the PUBLIC is reasonable,
            //   which is correct.
            //
            // * if we get 1, this saying we actually want element 0, so again -1 is
            //   correct. (This generalizes to all other "reasonable" values, but 1 is easiest
            //   to think about given the previous case's analysis.)
            //
            // * if the address we were looking for came *after* every FUNC, binary search
            //   would yield "slice.len()", and the nearest FUNC is indeed at `len-1`, so
            //   again correct.
            let funcs_slice = self.functions.ranges_values().as_slice();
            let prev_func = funcs_slice
                .binary_search_by_key(&addr, |(range, _)| range.start)
                .err()
                .and_then(|idx| idx.checked_sub(1))
                .and_then(|idx| funcs_slice.get(idx));

            if let Some(prev_func) = prev_func {
                if public.address <= prev_func.1.address {
                    // This PUBLIC is truncated by a FUNC before it gets to `addr`,
                    // so we shouldn't use it.
                    return;
                }
            }

            // Settle for a PUBLIC.
            frame.set_function(
                &public.name,
                public.address + module.base_address(),
                public.parameter_size,
            );
        }
    }

    pub fn walk_frame(&self, module: &dyn Module, walker: &mut dyn FrameWalker) -> Option<()> {
        if walker.get_instruction() < module.base_address() {
            return None;
        }
        let addr = walker.get_instruction() - module.base_address();

        // Preferentially use framedata over fpo, because if both are present,
        // the former tends to be more precise (breakpad heuristic).
        let win_stack_result = if let Some(info) = self.win_stack_framedata_info.get(addr) {
            walker::walk_with_stack_win_framedata(info, walker)
        } else if let Some(info) = self.win_stack_fpo_info.get(addr) {
            walker::walk_with_stack_win_fpo(info, walker)
        } else {
            None
        };

        // If STACK WIN failed, try STACK CFI
        win_stack_result.or_else(|| {
            if let Some(info) = self.cfi_stack_info.get(addr) {
                // Don't use add_rules that come after this address
                let mut count = 0;
                let len = info.add_rules.len();
                while count < len && info.add_rules[count].address <= addr {
                    count += 1;
                }

                walker::walk_with_stack_cfi(&info.init, &info.add_rules[0..count], walker)
            } else {
                None
            }
        })
    }

    /// Find the nearest `PublicSymbol` whose address is less than or equal to `addr`.
    pub fn find_nearest_public(&self, addr: u64) -> Option<&PublicSymbol> {
        self.publics.iter().rev().find(|&p| p.address <= addr)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::ffi::OsStr;
    fn test_symbolfile_from_file(rel_path: &str) {
        let mut path = std::env::current_dir().unwrap();
        if path.file_name() == Some(OsStr::new("rust-minidump")) {
            path.push("breakpad-symbols");
        }
        path.push(rel_path);
        let sym = SymbolFile::from_file(&path).unwrap();
        assert_eq!(sym.files.len(), 6661);
        assert_eq!(sym.publics.len(), 5);
        assert_eq!(sym.find_nearest_public(0x9b07).unwrap().name, "_NLG_Return");
        assert_eq!(
            sym.find_nearest_public(0x142e7).unwrap().name,
            "_NLG_Return"
        );
        assert_eq!(
            sym.find_nearest_public(0x23b06).unwrap().name,
            "__from_strstr_to_strchr"
        );
        assert_eq!(
            sym.find_nearest_public(0xFFFFFFFF).unwrap().name,
            "__from_strstr_to_strchr"
        );
        assert_eq!(sym.functions.ranges_values().count(), 1065);
        assert_eq!(sym.functions.get(0x1000).unwrap().name, "vswprintf");
        assert_eq!(sym.functions.get(0x1012).unwrap().name, "vswprintf");
        assert!(sym.functions.get(0x1013).is_none());
        // There are 1556 `STACK WIN 4` lines in the symbol file, but only 856
        // that don't overlap. However they all overlap in ways that we have
        // to handle in the wild.
        assert_eq!(sym.win_stack_framedata_info.ranges_values().count(), 1556);
        assert_eq!(sym.win_stack_fpo_info.ranges_values().count(), 259);
        assert_eq!(
            sym.win_stack_framedata_info.get(0x41b0).unwrap().address,
            0x41b0
        );
    }

    #[test]
    fn test_symbolfile_from_lf_file() {
        test_symbolfile_from_file(
            "testdata/symbols/test_app.pdb/5A9832E5287241C1838ED98914E9B7FF1/test_app.sym",
        );
    }

    #[test]
    fn test_symbolfile_from_crlf_file() {
        test_symbolfile_from_file(
            "testdata/symbols/test_app.pdb/6A9832E5287241C1838ED98914E9B7FF1/test_app.sym",
        );
    }

    fn test_symbolfile_from_bytes(symbolfile_bytes: &[u8]) {
        let sym = SymbolFile::from_bytes(symbolfile_bytes).unwrap();

        assert_eq!(sym.files.len(), 1);
        assert_eq!(sym.publics.len(), 1);
        assert_eq!(sym.functions.ranges_values().count(), 1);
        assert_eq!(sym.functions.get(0x1000).unwrap().name, "another func");
        assert_eq!(
            sym.functions
                .get(0x1000)
                .unwrap()
                .lines
                .ranges_values()
                .count(),
            1
        );
        // test fallback
        assert_eq!(sym.functions.get(0x1001).unwrap().name, "another func");
    }

    #[test]
    fn test_symbolfile_from_bytes_with_lf() {
        test_symbolfile_from_bytes(
            b"MODULE Linux x86 ffff0000 bar
FILE 53 bar.c
PUBLIC 1234 10 some public
FUNC 1000 30 10 another func
1000 30 7 53
",
        );
    }

    #[test]
    fn test_symbolfile_from_bytes_with_crlf() {
        test_symbolfile_from_bytes(
            b"MODULE Linux x86 ffff0000 bar
FILE 53 bar.c
PUBLIC 1234 10 some public
FUNC 1000 30 10 another func
1000 30 7 53
",
        );
    }
}
