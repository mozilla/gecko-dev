#![forbid(unsafe_code)]

use crate::{
    deflate::{
        fill_window, BlockState, DeflateStream, MIN_LOOKAHEAD, STD_MIN_MATCH, WANT_MIN_MATCH,
    },
    flush_block, DeflateFlush,
};

pub fn deflate_fast(stream: &mut DeflateStream, flush: DeflateFlush) -> BlockState {
    let mut bflush; /* set if current block must be flushed */
    let mut dist;
    let mut match_len = 0;

    loop {
        // Make sure that we always have enough lookahead, except
        // at the end of the input file. We need STD_MAX_MATCH bytes
        // for the next match, plus WANT_MIN_MATCH bytes to insert the
        // string following the next match.
        if stream.state.lookahead < MIN_LOOKAHEAD {
            fill_window(stream);
            if stream.state.lookahead < MIN_LOOKAHEAD && flush == DeflateFlush::NoFlush {
                return BlockState::NeedMore;
            }
            if stream.state.lookahead == 0 {
                break; /* flush the current block */
            }
        }

        let state = &mut stream.state;

        // Insert the string window[strstart .. strstart+2] in the
        // dictionary, and set hash_head to the head of the hash chain:

        if state.lookahead >= WANT_MIN_MATCH {
            let hash_head = state.quick_insert_string(state.strstart);
            dist = state.strstart as isize - hash_head as isize;

            /* Find the longest match, discarding those <= prev_length.
             * At this point we have always match length < WANT_MIN_MATCH
             */
            if dist <= state.max_dist() as isize && dist > 0 && hash_head != 0 {
                // To simplify the code, we prevent matches with the string
                // of window index 0 (in particular we have to avoid a match
                // of the string with itself at the start of the input file).
                (match_len, state.match_start) =
                    crate::deflate::longest_match::longest_match(state, hash_head);
            }
        }

        if match_len >= WANT_MIN_MATCH {
            // check_match(s, s->strstart, s->match_start, match_len);

            // bflush = zng_tr_tally_dist(s, s->strstart - s->match_start, match_len - STD_MIN_MATCH);
            bflush = state.tally_dist(
                state.strstart - state.match_start,
                match_len - STD_MIN_MATCH,
            );

            state.lookahead -= match_len;

            /* Insert new strings in the hash table only if the match length
             * is not too large. This saves time but degrades compression.
             */
            if match_len <= state.max_insert_length() && state.lookahead >= WANT_MIN_MATCH {
                match_len -= 1; /* string at strstart already in table */
                state.strstart += 1;

                state.insert_string(state.strstart, match_len);
                state.strstart += match_len;
            } else {
                state.strstart += match_len;
                state.quick_insert_string(state.strstart + 2 - STD_MIN_MATCH);

                /* If lookahead < STD_MIN_MATCH, ins_h is garbage, but it does not
                 * matter since it will be recomputed at next deflate call.
                 */
            }
            match_len = 0;
        } else {
            /* No match, output a literal byte */
            let lc = state.window.filled()[state.strstart];
            bflush = state.tally_lit(lc);
            state.lookahead -= 1;
            state.strstart += 1;
        }

        if bflush {
            flush_block!(stream, false);
        }
    }

    stream.state.insert = if stream.state.strstart < (STD_MIN_MATCH - 1) {
        stream.state.strstart
    } else {
        STD_MIN_MATCH - 1
    };

    if flush == DeflateFlush::Finish {
        flush_block!(stream, true);
        return BlockState::FinishDone;
    }

    if !stream.state.sym_buf.is_empty() {
        flush_block!(stream, false);
    }

    BlockState::BlockDone
}
