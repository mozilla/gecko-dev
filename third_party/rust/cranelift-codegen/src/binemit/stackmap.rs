use crate::bitset::BitSet;
use crate::ir;
use crate::isa::TargetIsa;
use alloc::vec::Vec;

type Num = u32;
const NUM_BITS: usize = core::mem::size_of::<Num>() * 8;

/// Wrapper class for longer bit vectors that cannot be represented by a single BitSet.
#[derive(Clone, Debug)]
pub struct Stackmap {
    bitmap: Vec<BitSet<Num>>,
}

impl Stackmap {
    /// Create a stackmap based on where references are located on a function's stack.
    pub fn from_values(
        args: &[ir::entities::Value],
        func: &ir::Function,
        isa: &dyn TargetIsa,
    ) -> Self {
        let loc = &func.locations;
        let mut live_ref_in_stack_slot = crate::HashSet::new();
        // References can be in registers, and live registers values are pushed onto the stack before calls and traps.
        // TODO: Implement register maps. If a register containing a reference is spilled and reused after a safepoint,
        // it could contain a stale reference value if the garbage collector relocated the value.
        for val in args {
            if let Some(value_loc) = loc.get(*val) {
                match *value_loc {
                    ir::ValueLoc::Stack(stack_slot) => live_ref_in_stack_slot.insert(stack_slot),
                    _ => false,
                };
            }
        }

        // SpiderMonkey stackmap structure:
        // <trap reg dump> + <general spill> + <frame> + <inbound args>
        // Bit vector goes from lower addresses to higher addresses.

        // TODO: Get trap register layout from Spidermonkey and prepend to bitvector below.
        let stack = &func.stack_slots;
        let frame_size = stack.frame_size.unwrap();
        let word_size = ir::stackslot::StackSize::from(isa.pointer_bytes());
        let num_words = (frame_size / word_size) as usize;
        let mut vec = alloc::vec::Vec::with_capacity(num_words);

        vec.resize(num_words, false);

        // Frame (includes spills and inbound args).
        for (ss, ssd) in stack.iter() {
            if live_ref_in_stack_slot.contains(&ss) {
                // Assumption: greater magnitude of offset imply higher address.
                let index = (((ssd.offset.unwrap().abs() as u32) - ssd.size) / word_size) as usize;
                vec[index] = true;
            }
        }

        Stackmap::from_slice(&vec)
    }

    /// Create a vec of Bitsets from a slice of bools.
    pub fn from_slice(vec: &[bool]) -> Self {
        let len = vec.len();
        let num_word = len / NUM_BITS + (len % NUM_BITS != 0) as usize;
        let mut bitmap = Vec::with_capacity(num_word);

        for segment in vec.chunks(NUM_BITS) {
            let mut curr_word = 0;
            for (i, set) in segment.iter().enumerate() {
                if *set {
                    curr_word |= 1 << i;
                }
            }
            bitmap.push(BitSet(curr_word));
        }
        Self { bitmap }
    }

    /// Returns a specified bit.
    pub fn get_bit(&self, bit_index: usize) -> bool {
        assert!(bit_index < NUM_BITS * self.bitmap.len());
        let word_index = bit_index / NUM_BITS;
        let word_offset = (bit_index % NUM_BITS) as u8;
        self.bitmap[word_index].contains(word_offset)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn stackmaps() {
        let vec: Vec<bool> = Vec::new();
        assert!(Stackmap::from_slice(&vec).bitmap.is_empty());

        let mut vec: [bool; NUM_BITS] = Default::default();
        let set_true_idx = [5, 7, 24, 31];

        for &idx in &set_true_idx {
            vec[idx] = true;
        }

        let mut vec = vec.to_vec();
        assert_eq!(
            vec![BitSet::<Num>(2164261024)],
            Stackmap::from_slice(&vec).bitmap
        );

        vec.push(false);
        vec.push(true);
        let res = Stackmap::from_slice(&vec);
        assert_eq!(
            vec![BitSet::<Num>(2164261024), BitSet::<Num>(2)],
            res.bitmap
        );

        assert!(res.get_bit(5));
        assert!(res.get_bit(31));
        assert!(res.get_bit(33));
        assert!(!res.get_bit(1));
    }
}
