use super::unwindregs::Reg;
use arrayvec::ArrayVec;

const ENCODE_REGISTERS: [Reg; 8] = [
    Reg::RBX,
    Reg::RBP,
    Reg::RDI,
    Reg::RSI,
    Reg::R12,
    Reg::R13,
    Reg::R14,
    Reg::R15,
];

pub fn decode(count: u8, encoded_ordering: u16) -> ArrayVec<Reg, 8> {
    let mut regs: ArrayVec<Reg, 8> = ENCODE_REGISTERS.into();
    let mut r = encoded_ordering;
    let mut n: u16 = 8;
    while r != 0 {
        let index = r % n;
        if index != 0 {
            regs[(8 - n as usize)..].swap(index as usize, 0);
        }
        r /= n;
        n -= 1;
    }
    regs.truncate(count as usize);
    regs
}

pub fn encode(registers: &[Reg]) -> Option<(u8, u16)> {
    if registers.len() > ENCODE_REGISTERS.len() {
        return None;
    }

    let count = registers.len() as u8;
    let mut r: u16 = 0;
    let mut reg_order: ArrayVec<Reg, 8> = ENCODE_REGISTERS.into();

    let mut scale: u16 = 1;
    for (i, reg) in registers.iter().enumerate() {
        let index = reg_order[i..].iter().position(|r| r == reg)?;
        if index as u16 != 0 {
            reg_order[i..].swap(index, 0);
        }
        r += index as u16 * scale;
        scale *= 8 - i as u16;
    }
    Some((count, r))
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn unhandled_orderings() {
        use super::Reg::*;

        assert_eq!(encode(&[RAX]), None, "RAX is a volatile register, i.e. not a callee-save register, so it does not need to be restored during epilogs and is not covered by the encoding.");
        assert_eq!(encode(&[RSI, RSI]), None, "Valid register orderings only contain each register (at most) once, so there is no encoding for a sequence with repeated registers.");
    }

    #[test]
    fn roundtrip_all() {
        // Test all possible register orderings.
        // That is, for all permutations of length 0 to 8 of the ENCODE_REGISTERS array, check that
        // the register ordering rountrips successfully through encoding and decoding.
        use itertools::Itertools;
        for permutation in (0..=8).flat_map(|k| ENCODE_REGISTERS.iter().cloned().permutations(k)) {
            let permutation = permutation.as_slice();
            let encoding = encode(permutation);
            if let Some((count, encoded)) = encoding {
                assert_eq!(
                    decode(count, encoded).as_slice(),
                    permutation,
                    "Register permutation should roundtrip correctly",
                );
            } else {
                panic!("Register permutation failed to encode: {permutation:?}");
            }
        }
    }
}
