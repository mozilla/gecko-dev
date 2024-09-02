use crate::deflate::{State, MIN_LOOKAHEAD, STD_MAX_MATCH, STD_MIN_MATCH};

type Pos = u16;

const EARLY_EXIT_TRIGGER_LEVEL: i8 = 5;

const UNALIGNED_OK: bool = cfg!(any(
    target_arch = "x86",
    target_arch = "x86_64",
    target_arch = "arm",
    target_arch = "aarch64",
    target_arch = "powerpc64",
));

const UNALIGNED64_OK: bool = cfg!(any(
    target_arch = "x86_64",
    target_arch = "aarch64",
    target_arch = "powerpc64",
));

pub fn longest_match(state: &crate::deflate::State, cur_match: u16) -> (usize, usize) {
    longest_match_help::<false>(state, cur_match)
}

pub fn longest_match_slow(state: &crate::deflate::State, cur_match: u16) -> (usize, usize) {
    longest_match_help::<true>(state, cur_match)
}

fn longest_match_help<const SLOW: bool>(
    state: &crate::deflate::State,
    mut cur_match: u16,
) -> (usize, usize) {
    let mut match_start = state.match_start;

    let strstart = state.strstart;
    let wmask = state.w_mask;
    let window = state.window.filled();
    let scan = &window[strstart..];
    let mut limit: Pos;
    let limit_base: Pos;
    let early_exit: bool;

    let mut chain_length: usize;
    let mut best_len: usize;

    let lookahead = state.lookahead;
    let mut match_offset = 0;

    let mut scan_start = [0u8; 8];
    let mut scan_end = [0u8; 8];

    macro_rules! goto_next_in_chain {
        () => {
            chain_length -= 1;
            if chain_length > 0 {
                cur_match = state.prev[cur_match as usize & wmask];

                if cur_match > limit {
                    continue;
                }
            }

            return (best_len, match_start);
        };
    }

    // The code is optimized for STD_MAX_MATCH-2 multiple of 16.
    assert_eq!(STD_MAX_MATCH, 258, "Code too clever");

    best_len = if state.prev_length > 0 {
        state.prev_length
    } else {
        STD_MIN_MATCH - 1
    };

    // Calculate read offset which should only extend an extra byte to find the next best match length.
    let mut offset = best_len - 1;
    if best_len >= core::mem::size_of::<u32>() && UNALIGNED_OK {
        offset -= 2;
        if best_len >= core::mem::size_of::<u64>() && UNALIGNED64_OK {
            offset -= 4;
        }
    }

    if UNALIGNED64_OK {
        scan_start.copy_from_slice(&scan[..core::mem::size_of::<u64>()]);
        scan_end.copy_from_slice(&scan[offset..][..core::mem::size_of::<u64>()]);
    } else if UNALIGNED_OK {
        scan_start[..4].copy_from_slice(&scan[..core::mem::size_of::<u32>()]);
        scan_end[..4].copy_from_slice(&scan[offset..][..core::mem::size_of::<u32>()]);
    } else {
        scan_start[..2].copy_from_slice(&scan[..core::mem::size_of::<u16>()]);
        scan_end[..2].copy_from_slice(&scan[offset..][..core::mem::size_of::<u16>()]);
    }

    let mut mbase_start = window.as_ptr();
    let mut mbase_end = window[offset..].as_ptr();

    // Don't waste too much time by following a chain if we already have a good match
    chain_length = state.max_chain_length;
    if best_len >= state.good_match {
        chain_length >>= 2;
    }
    let nice_match = state.nice_match;

    // Stop when cur_match becomes <= limit. To simplify the code,
    // we prevent matches with the string of window index 0
    limit = strstart.saturating_sub(state.max_dist()) as Pos;

    // look for a better string offset
    if SLOW {
        limit_base = limit;

        if best_len >= STD_MIN_MATCH {
            /* We're continuing search (lazy evaluation). */
            let mut pos: Pos;

            // Find a most distant chain starting from scan with index=1 (index=0 corresponds
            // to cur_match). We cannot use s->prev[strstart+1,...] immediately, because
            // these strings are not yet inserted into the hash table.
            let Some([_cur_match, scan1, scan2, scanrest @ ..]) = scan.get(..best_len + 1) else {
                panic!("invalid scan");
            };

            let mut hash = 0;
            hash = state.update_hash(hash, *scan1 as u32);
            hash = state.update_hash(hash, *scan2 as u32);

            for (i, b) in scanrest.iter().enumerate() {
                hash = state.update_hash(hash, *b as u32);

                /* If we're starting with best_len >= 3, we can use offset search. */
                pos = state.head[hash as usize];
                if pos < cur_match {
                    match_offset = (i + 1) as Pos;
                    cur_match = pos;
                }
            }

            /* Update offset-dependent variables */
            limit = limit_base + match_offset;
            if cur_match <= limit {
                return break_matching(state, best_len, match_start);
            }

            mbase_start = mbase_start.wrapping_sub(match_offset as usize);
            mbase_end = mbase_end.wrapping_sub(match_offset as usize);
        }

        early_exit = false;
    } else {
        // must initialize this variable
        limit_base = 0;
        early_exit = state.level < EARLY_EXIT_TRIGGER_LEVEL;
    }

    assert!(
        strstart <= state.window_size - MIN_LOOKAHEAD,
        "need lookahead"
    );

    loop {
        if cur_match as usize >= strstart {
            break;
        }

        // Skip to next match if the match length cannot increase or if the match length is
        // less than 2. Note that the checks below for insufficient lookahead only occur
        // occasionally for performance reasons.
        // Therefore uninitialized memory will be accessed and conditional jumps will be made
        // that depend on those values. However the length of the match is limited to the
        // lookahead, so the output of deflate is not affected by the uninitialized values.

        // # Safety
        //
        // The two pointers must be valid for reads of N bytes.
        #[inline(always)]
        unsafe fn memcmp_n_ptr<const N: usize>(src0: *const u8, src1: *const u8) -> bool {
            let src0_cmp = core::ptr::read(src0 as *const [u8; N]);
            let src1_cmp = core::ptr::read(src1 as *const [u8; N]);

            src0_cmp == src1_cmp
        }

        #[inline(always)]
        unsafe fn is_match<const N: usize>(
            cur_match: u16,
            mbase_start: *const u8,
            mbase_end: *const u8,
            scan_start: *const u8,
            scan_end: *const u8,
        ) -> bool {
            let be = mbase_end.wrapping_add(cur_match as usize);
            let bs = mbase_start.wrapping_add(cur_match as usize);

            memcmp_n_ptr::<N>(be, scan_end) && memcmp_n_ptr::<N>(bs, scan_start)
        }

        // first, do a quick check on the start and end bytes. Go to the next item in the chain if
        // these bytes don't match.
        unsafe {
            let scan_start = scan_start.as_ptr();
            let scan_end = scan_end.as_ptr();

            if UNALIGNED_OK {
                if best_len < core::mem::size_of::<u32>() {
                    loop {
                        if is_match::<2>(cur_match, mbase_start, mbase_end, scan_start, scan_end) {
                            break;
                        }

                        goto_next_in_chain!();
                    }
                } else if best_len >= core::mem::size_of::<u64>() && UNALIGNED64_OK {
                    loop {
                        if is_match::<8>(cur_match, mbase_start, mbase_end, scan_start, scan_end) {
                            break;
                        }

                        goto_next_in_chain!();
                    }
                } else {
                    loop {
                        if is_match::<4>(cur_match, mbase_start, mbase_end, scan_start, scan_end) {
                            break;
                        }

                        goto_next_in_chain!();
                    }
                }
            } else {
                loop {
                    if memcmp_n_ptr::<2>(mbase_end.wrapping_add(cur_match as usize), scan_end)
                        && memcmp_n_ptr::<2>(
                            mbase_start.wrapping_add(cur_match as usize),
                            scan.as_ptr(),
                        )
                    {
                        break;
                    }

                    goto_next_in_chain!();
                }
            }
        }

        // we know that there is at least some match. Now count how many bytes really match
        let len = {
            // TODO this just looks so incredibly unsafe!
            let src1: &[u8; 256] =
                unsafe { &*mbase_start.wrapping_add(cur_match as usize + 2).cast() };

            crate::deflate::compare256::compare256_slice(&scan[2..], src1) + 2
        };

        assert!(
            scan.as_ptr() as usize + len <= window.as_ptr() as usize + (state.window_size - 1),
            "wild scan"
        );

        if len > best_len {
            match_start = (cur_match - match_offset) as usize;

            /* Do not look for matches beyond the end of the input. */
            if len > lookahead {
                return (lookahead, match_start);
            }
            best_len = len;
            if best_len >= nice_match {
                return (best_len, match_start);
            }

            offset = best_len - 1;
            if best_len >= core::mem::size_of::<u32>() && UNALIGNED_OK {
                offset -= 2;
                if best_len >= core::mem::size_of::<u64>() && UNALIGNED64_OK {
                    offset -= 4;
                }
            }

            if UNALIGNED64_OK {
                scan_end.copy_from_slice(&scan[offset..][..core::mem::size_of::<u64>()]);
            } else if UNALIGNED_OK {
                scan_end[..4].copy_from_slice(&scan[offset..][..core::mem::size_of::<u32>()]);
            } else {
                scan_end[..2].copy_from_slice(&scan[offset..][..core::mem::size_of::<u16>()]);
            }

            // Look for a better string offset
            if SLOW && len > STD_MIN_MATCH && match_start + len < strstart {
                let mut pos: Pos;
                // uint32_t i, hash;
                // unsigned char *scan_endstr;

                /* Go back to offset 0 */
                cur_match -= match_offset;
                match_offset = 0;
                let mut next_pos = cur_match;

                for i in 0..=len - STD_MIN_MATCH {
                    pos = state.prev[(cur_match as usize + i) & wmask];
                    if pos < next_pos {
                        /* Hash chain is more distant, use it */
                        if pos <= limit_base + i as Pos {
                            return break_matching(state, best_len, match_start);
                        }
                        next_pos = pos;
                        match_offset = i as Pos;
                    }
                }
                /* Switch cur_match to next_pos chain */
                cur_match = next_pos;

                /* Try hash head at len-(STD_MIN_MATCH-1) position to see if we could get
                 * a better cur_match at the end of string. Using (STD_MIN_MATCH-1) lets
                 * us include one more byte into hash - the byte which will be checked
                 * in main loop now, and which allows to grow match by 1.
                 */
                let [scan0, scan1, scan2, ..] = scan[len - (STD_MIN_MATCH + 1)..] else {
                    panic!("index out of bounds");
                };

                let mut hash = 0;
                hash = state.update_hash(hash, scan0 as u32);
                hash = state.update_hash(hash, scan1 as u32);
                hash = state.update_hash(hash, scan2 as u32);

                pos = state.head[hash as usize];
                if pos < cur_match {
                    match_offset = (len - (STD_MIN_MATCH + 1)) as Pos;
                    if pos <= limit_base + match_offset {
                        return break_matching(state, best_len, match_start);
                    }
                    cur_match = pos;
                }

                /* Update offset-dependent variables */
                limit = limit_base + match_offset;
                mbase_start = window.as_ptr().wrapping_sub(match_offset as usize);
                mbase_end = mbase_start.wrapping_add(offset);
                continue;
            }

            mbase_end = mbase_start.wrapping_add(offset);
        } else if !SLOW && early_exit {
            // The probability of finding a match later if we here is pretty low, so for
            // performance it's best to outright stop here for the lower compression levels
            break;
        }

        goto_next_in_chain!();
    }

    (best_len, match_start)
}

fn break_matching(state: &State, best_len: usize, match_start: usize) -> (usize, usize) {
    (Ord::min(best_len, state.lookahead), match_start)
}
