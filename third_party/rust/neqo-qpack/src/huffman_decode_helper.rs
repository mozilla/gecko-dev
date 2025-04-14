// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::sync::OnceLock;

use crate::huffman_table::HUFFMAN_TABLE;

// Since we're encoding the table length as a u16, we need to ensure that it fits.
static_assertions::const_assert!(HUFFMAN_TABLE.len() <= u16::MAX as usize);

pub struct HuffmanDecoderNode {
    pub next: [Option<Box<HuffmanDecoderNode>>; 2],
    pub value: Option<u16>,
}

pub fn huffman_decoder_root() -> &'static HuffmanDecoderNode {
    static ROOT: OnceLock<HuffmanDecoderNode> = OnceLock::new();
    ROOT.get_or_init(|| make_huffman_tree(0, 0))
}

fn make_huffman_tree(prefix: u32, len: u8) -> HuffmanDecoderNode {
    let mut found = false;
    let mut next = [None, None];
    for (i, iter) in HUFFMAN_TABLE.iter().enumerate() {
        if iter.len <= len {
            continue;
        }
        if (iter.val >> (iter.len - len)) != prefix {
            continue;
        }

        found = true;
        if iter.len == len + 1 {
            // This is a leaf
            let bit = usize::try_from(iter.val & 1).expect("u32 fits in usize");
            next[bit] = Some(Box::new(HuffmanDecoderNode {
                next: [None, None],
                #[expect(
                    clippy::cast_possible_truncation,
                    reason = "We've checked this in a `const_assert!` above."
                )]
                value: Some(i as u16),
            }));
            if next[bit ^ 1].is_some() {
                return HuffmanDecoderNode { next, value: None };
            }
        }
    }

    if found {
        if next[0].is_none() {
            next[0] = Some(Box::new(make_huffman_tree(prefix << 1, len + 1)));
        }
        if next[1].is_none() {
            next[1] = Some(Box::new(make_huffman_tree((prefix << 1) + 1, len + 1)));
        }
    }

    HuffmanDecoderNode { next, value: None }
}
