#![warn(unsafe_op_in_unsafe_fn)]
use crate::deflate::{State, HASH_SIZE, STD_MIN_MATCH};

#[derive(Debug, Clone, Copy)]
pub enum HashCalcVariant {
    Standard,
    Crc32,
    Roll,
}

impl HashCalcVariant {
    #[cfg(test)]
    pub fn for_compression_level(level: usize) -> Self {
        let max_chain_length = crate::deflate::algorithm::CONFIGURATION_TABLE[level].max_chain;
        Self::for_max_chain_length(max_chain_length as usize)
    }

    /// Use rolling hash for deflate_slow algorithm with level 9. It allows us to
    /// properly lookup different hash chains to speed up longest_match search.
    pub fn for_max_chain_length(max_chain_length: usize) -> Self {
        if max_chain_length > 1024 {
            HashCalcVariant::Roll
        } else if Crc32HashCalc::is_supported() {
            HashCalcVariant::Crc32
        } else {
            HashCalcVariant::Standard
        }
    }
}

pub struct StandardHashCalc;

impl StandardHashCalc {
    const HASH_CALC_OFFSET: usize = 0;

    const HASH_CALC_MASK: u32 = (HASH_SIZE - 1) as u32;

    fn hash_calc(_: u32, val: u32) -> u32 {
        const HASH_SLIDE: u32 = 16;
        val.wrapping_mul(2654435761) >> HASH_SLIDE
    }

    pub fn update_hash(h: u32, val: u32) -> u32 {
        Self::hash_calc(h, val) & Self::HASH_CALC_MASK
    }

    pub fn quick_insert_string(state: &mut State, string: usize) -> u16 {
        let slice = &state.window.filled()[string + Self::HASH_CALC_OFFSET..];
        let val = u32::from_le_bytes(slice[..4].try_into().unwrap());

        let hm = Self::update_hash(0, val) as usize;

        let head = state.head[hm];
        if head != string as u16 {
            state.prev[string & state.w_mask] = head;
            state.head[hm] = string as u16;
        }

        head
    }

    pub fn insert_string(state: &mut State, string: usize, count: usize) {
        let slice = &state.window.filled()[string + Self::HASH_CALC_OFFSET..];

        // .take(count) generates worse assembly
        for (i, w) in slice[..count + 3].windows(4).enumerate() {
            let idx = string as u16 + i as u16;

            let val = u32::from_le_bytes(w.try_into().unwrap());

            let hm = Self::update_hash(0, val) as usize;

            let head = state.head[hm];
            if head != idx {
                state.prev[idx as usize & state.w_mask] = head;
                state.head[hm] = idx;
            }
        }
    }
}

pub struct RollHashCalc;

impl RollHashCalc {
    const HASH_CALC_OFFSET: usize = STD_MIN_MATCH - 1;

    const HASH_CALC_MASK: u32 = (1 << 15) - 1;

    fn hash_calc(h: u32, val: u32) -> u32 {
        const HASH_SLIDE: u32 = 5;
        (h << HASH_SLIDE) ^ val
    }

    pub fn update_hash(h: u32, val: u32) -> u32 {
        Self::hash_calc(h, val) & Self::HASH_CALC_MASK
    }

    pub fn quick_insert_string(state: &mut State, string: usize) -> u16 {
        let val = state.window.filled()[string + Self::HASH_CALC_OFFSET] as u32;

        state.ins_h = Self::hash_calc(state.ins_h as u32, val) as usize;
        state.ins_h &= Self::HASH_CALC_MASK as usize;

        let hm = state.ins_h;

        let head = state.head[hm];
        if head != string as u16 {
            state.prev[string & state.w_mask] = head;
            state.head[hm] = string as u16;
        }

        head
    }

    pub fn insert_string(state: &mut State, string: usize, count: usize) {
        let slice = &state.window.filled()[string + Self::HASH_CALC_OFFSET..][..count];

        for (i, val) in slice.iter().copied().enumerate() {
            let idx = string as u16 + i as u16;

            state.ins_h = Self::hash_calc(state.ins_h as u32, val as u32) as usize;
            state.ins_h &= Self::HASH_CALC_MASK as usize;
            let hm = state.ins_h;

            let head = state.head[hm];
            if head != idx {
                state.prev[idx as usize & state.w_mask] = head;
                state.head[hm] = idx;
            }
        }
    }
}

pub struct Crc32HashCalc;

impl Crc32HashCalc {
    fn is_supported() -> bool {
        #[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), feature = "std"))]
        return std::arch::is_x86_feature_detected!("sse4.2");

        // NOTE: more recent versions of zlib-ng no longer use the crc instructions on aarch64
        #[cfg(all(target_arch = "aarch64", feature = "std"))]
        return std::arch::is_aarch64_feature_detected!("crc");

        #[allow(unreachable_code)]
        false
    }

    const HASH_CALC_OFFSET: usize = 0;

    const HASH_CALC_MASK: u32 = (HASH_SIZE - 1) as u32;

    #[cfg(target_arch = "x86")]
    #[target_feature(enable = "sse4.2")]
    unsafe fn hash_calc(h: u32, val: u32) -> u32 {
        unsafe { core::arch::x86::_mm_crc32_u32(h, val) }
    }

    #[cfg(target_arch = "x86_64")]
    #[target_feature(enable = "sse4.2")]
    unsafe fn hash_calc(h: u32, val: u32) -> u32 {
        unsafe { core::arch::x86_64::_mm_crc32_u32(h, val) }
    }

    #[cfg(target_arch = "aarch64")]
    #[target_feature(enable = "neon")]
    unsafe fn hash_calc(h: u32, val: u32) -> u32 {
        unsafe { crate::crc32::acle::__crc32w(h, val) }
    }

    #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")))]
    unsafe fn hash_calc(_h: u32, _val: u32) -> u32 {
        assert!(!Self::is_supported());
        unimplemented!("there is no hardware support on this platform")
    }

    #[cfg_attr(target_arch = "aarch64", target_feature(enable = "neon"))]
    #[cfg_attr(target_arch = "x86", target_feature(enable = "sse4.2"))]
    #[cfg_attr(target_arch = "x86_64", target_feature(enable = "sse4.2"))]
    pub unsafe fn update_hash(h: u32, val: u32) -> u32 {
        (unsafe { Self::hash_calc(h, val) }) & Self::HASH_CALC_MASK
    }

    #[cfg_attr(target_arch = "aarch64", target_feature(enable = "neon"))]
    #[cfg_attr(target_arch = "x86", target_feature(enable = "sse4.2"))]
    #[cfg_attr(target_arch = "x86_64", target_feature(enable = "sse4.2"))]
    pub unsafe fn quick_insert_string(state: &mut State, string: usize) -> u16 {
        let slice = &state.window.filled()[string + Self::HASH_CALC_OFFSET..];
        let val = u32::from_le_bytes(slice[..4].try_into().unwrap());

        let hm = unsafe { Self::update_hash(0, val) } as usize;

        let head = state.head[hm];
        if head != string as u16 {
            state.prev[string & state.w_mask] = head;
            state.head[hm] = string as u16;
        }

        head
    }

    #[cfg_attr(target_arch = "aarch64", target_feature(enable = "neon"))]
    #[cfg_attr(target_arch = "x86", target_feature(enable = "sse4.2"))]
    #[cfg_attr(target_arch = "x86_64", target_feature(enable = "sse4.2"))]
    pub unsafe fn insert_string(state: &mut State, string: usize, count: usize) {
        let slice = &state.window.filled()[string + Self::HASH_CALC_OFFSET..];

        // .take(count) generates worse assembly
        for (i, w) in slice[..count + 3].windows(4).enumerate() {
            let idx = string as u16 + i as u16;

            let val = u32::from_le_bytes(w.try_into().unwrap());

            let hm = unsafe { Self::update_hash(0, val) } as usize;

            let head = state.head[hm];
            if head != idx {
                state.prev[idx as usize & state.w_mask] = head;
                state.head[hm] = idx;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg_attr(
        not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")),
        ignore = "no crc32 hardware support on this platform"
    )]
    fn crc32_hash_calc() {
        if !Crc32HashCalc::is_supported() {
            return;
        }

        unsafe {
            if cfg!(target_arch = "x86") || cfg!(target_arch = "x86_64") {
                assert_eq!(Crc32HashCalc::hash_calc(0, 807411760), 2423125009);
                assert_eq!(Crc32HashCalc::hash_calc(0, 540024864), 1452438466);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538980384), 435552201);
                assert_eq!(Crc32HashCalc::hash_calc(0, 807411760), 2423125009);
                assert_eq!(Crc32HashCalc::hash_calc(0, 540024864), 1452438466);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538980384), 435552201);
                assert_eq!(Crc32HashCalc::hash_calc(0, 807411760), 2423125009);
                assert_eq!(Crc32HashCalc::hash_calc(0, 540024864), 1452438466);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538980384), 435552201);
                assert_eq!(Crc32HashCalc::hash_calc(0, 807411760), 2423125009);
                assert_eq!(Crc32HashCalc::hash_calc(0, 540024864), 1452438466);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538980384), 435552201);
                assert_eq!(Crc32HashCalc::hash_calc(0, 807411760), 2423125009);
                assert_eq!(Crc32HashCalc::hash_calc(0, 170926112), 500028708);
                assert_eq!(Crc32HashCalc::hash_calc(0, 537538592), 3694129053);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538970672), 373925026);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538976266), 4149335727);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538976288), 1767342659);
                assert_eq!(Crc32HashCalc::hash_calc(0, 941629472), 4090502627);
                assert_eq!(Crc32HashCalc::hash_calc(0, 775430176), 1744703325);
            } else {
                assert_eq!(Crc32HashCalc::hash_calc(0, 807411760), 2067507791);
                assert_eq!(Crc32HashCalc::hash_calc(0, 540024864), 2086141925);
                assert_eq!(Crc32HashCalc::hash_calc(0, 538980384), 716394180);
                assert_eq!(Crc32HashCalc::hash_calc(0, 775430176), 1396070634);
                assert_eq!(Crc32HashCalc::hash_calc(0, 941629472), 637105634);
            }
        }
    }

    #[test]
    fn roll_hash_calc() {
        assert_eq!(RollHashCalc::hash_calc(2565, 93), 82173);
        assert_eq!(RollHashCalc::hash_calc(16637, 10), 532394);
        assert_eq!(RollHashCalc::hash_calc(8106, 100), 259364);
        assert_eq!(RollHashCalc::hash_calc(29988, 101), 959717);
        assert_eq!(RollHashCalc::hash_calc(9445, 98), 302274);
        assert_eq!(RollHashCalc::hash_calc(7362, 117), 235573);
        assert_eq!(RollHashCalc::hash_calc(6197, 103), 198343);
        assert_eq!(RollHashCalc::hash_calc(1735, 32), 55488);
        assert_eq!(RollHashCalc::hash_calc(22720, 61), 727101);
        assert_eq!(RollHashCalc::hash_calc(6205, 32), 198528);
        assert_eq!(RollHashCalc::hash_calc(3826, 117), 122421);
        assert_eq!(RollHashCalc::hash_calc(24117, 101), 771781);
    }

    #[test]
    fn standard_hash_calc() {
        assert_eq!(StandardHashCalc::hash_calc(0, 807411760), 65468);
        assert_eq!(StandardHashCalc::hash_calc(0, 540024864), 42837);
        assert_eq!(StandardHashCalc::hash_calc(0, 538980384), 33760);
        assert_eq!(StandardHashCalc::hash_calc(0, 775430176), 8925);
        assert_eq!(StandardHashCalc::hash_calc(0, 941629472), 42053);
    }
}
