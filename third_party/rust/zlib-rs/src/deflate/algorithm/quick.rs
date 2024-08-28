#![forbid(unsafe_code)]

use crate::{
    deflate::{
        fill_window, flush_pending, BlockState, BlockType, DeflateStream, State, StaticTreeDesc,
        MIN_LOOKAHEAD, STD_MAX_MATCH, STD_MIN_MATCH, WANT_MIN_MATCH,
    },
    DeflateFlush,
};

pub fn deflate_quick(stream: &mut DeflateStream, flush: DeflateFlush) -> BlockState {
    let mut state = &mut stream.state;

    let last = matches!(flush, DeflateFlush::Finish);

    macro_rules! quick_start_block {
        () => {
            state.bit_writer.emit_tree(BlockType::StaticTrees, last);
            state.block_open = 1 + last as u8;
            state.block_start = state.strstart as isize;
        };
    }

    macro_rules! quick_end_block {
        ($last:expr) => {
            if state.block_open > 0 {
                state
                    .bit_writer
                    .emit_end_block_and_align(&StaticTreeDesc::L.static_tree, $last);
                state.block_open = 0;
                state.block_start = state.strstart as isize;
                flush_pending(stream);
                #[allow(unused_assignments)]
                {
                    state = &mut stream.state;
                }
                if stream.avail_out == 0 {
                    return match last {
                        true => BlockState::FinishStarted,
                        false => BlockState::NeedMore,
                    };
                }
            }
        };
    }

    if last && state.block_open != 2 {
        /* Emit end of previous block */
        quick_end_block!(false);
        /* Emit start of last block */
        quick_start_block!();
    } else if state.block_open == 0 && state.lookahead > 0 {
        /* Start new block only when we have lookahead data, so that if no
        input data is given an empty block will not be written */
        quick_start_block!();
    }

    loop {
        if state.bit_writer.pending.pending + State::BIT_BUF_SIZE.div_ceil(8) as usize
            >= state.pending_buf_size()
        {
            flush_pending(stream);
            state = &mut stream.state;
            if stream.avail_out == 0 {
                return if last
                    && stream.avail_in == 0
                    && state.bit_writer.bits_used == 0
                    && state.block_open == 0
                {
                    BlockState::FinishStarted
                } else {
                    BlockState::NeedMore
                };
            }
        }

        if state.lookahead < MIN_LOOKAHEAD {
            fill_window(stream);
            state = &mut stream.state;

            if state.lookahead < MIN_LOOKAHEAD && matches!(flush, DeflateFlush::NoFlush) {
                return BlockState::NeedMore;
            }
            if state.lookahead == 0 {
                break;
            }

            if state.block_open == 0 {
                // Start new block when we have lookahead data,
                // so that if no input data is given an empty block will not be written
                quick_start_block!();
            }
        }

        if state.lookahead >= WANT_MIN_MATCH {
            let hash_head = state.quick_insert_string(state.strstart);
            let dist = state.strstart as isize - hash_head as isize;

            if dist <= state.max_dist() as isize && dist > 0 {
                let str_start = &state.window.filled()[state.strstart..];
                let match_start = &state.window.filled()[hash_head as usize..];

                if str_start[0] == match_start[0] && str_start[1] == match_start[1] {
                    let mut match_len = crate::deflate::compare256::compare256_slice(
                        &str_start[2..],
                        &match_start[2..],
                    ) + 2;

                    if match_len >= WANT_MIN_MATCH {
                        match_len = Ord::min(match_len, state.lookahead);
                        match_len = Ord::min(match_len, STD_MAX_MATCH);

                        // TODO do this with a debug_assert?
                        // check_match(s, state.strstart, hash_head, match_len);

                        state.bit_writer.emit_dist(
                            StaticTreeDesc::L.static_tree,
                            StaticTreeDesc::D.static_tree,
                            (match_len - STD_MIN_MATCH) as u8,
                            dist as usize,
                        );
                        state.lookahead -= match_len;
                        state.strstart += match_len;
                        continue;
                    }
                }
            }
        }

        let lc = state.window.filled()[state.strstart];
        state.bit_writer.emit_lit(StaticTreeDesc::L.static_tree, lc);
        state.strstart += 1;
        state.lookahead -= 1;
    }

    state.insert = Ord::min(state.strstart, STD_MIN_MATCH - 1);

    quick_end_block!(last);

    if last {
        BlockState::FinishDone
    } else {
        BlockState::BlockDone
    }
}
