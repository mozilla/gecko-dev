use core::mem::MaybeUninit;

use crate::{read_buf::ReadBuf, CRC32_INITIAL_VALUE};

#[cfg(target_arch = "aarch64")]
pub(crate) mod acle;
mod braid;
mod combine;
#[cfg(target_arch = "x86_64")]
mod pclmulqdq;

pub use combine::crc32_combine;

pub fn crc32(start: u32, buf: &[u8]) -> u32 {
    /* For lens < 64, crc32_braid method is faster. The CRC32 instruction for
     * these short lengths might also prove to be effective */
    if buf.len() < 64 {
        return crc32_braid(start, buf);
    }

    let mut crc_state = Crc32Fold::new_with_initial(start);
    crc_state.fold(buf, start);
    crc_state.finish()
}

pub fn crc32_braid(start: u32, buf: &[u8]) -> u32 {
    braid::crc32_braid::<5>(start, buf)
}

#[allow(unused)]
pub fn crc32_copy(dst: &mut ReadBuf, buf: &[u8]) -> u32 {
    /* For lens < 64, crc32_braid method is faster. The CRC32 instruction for
     * these short lengths might also prove to be effective */
    if buf.len() < 64 {
        dst.extend(buf);
        return braid::crc32_braid::<5>(CRC32_INITIAL_VALUE, buf);
    }

    let mut crc_state = Crc32Fold::new();

    crc_state.fold_copy(unsafe { dst.inner_mut() }, buf);
    unsafe { dst.assume_init(buf.len()) };
    dst.set_filled(buf.len());

    crc_state.finish()
}

#[derive(Debug, Clone, Copy)]
pub struct Crc32Fold {
    #[cfg(target_arch = "x86_64")]
    fold: pclmulqdq::Accumulator,
    value: u32,
}

impl Default for Crc32Fold {
    fn default() -> Self {
        Self::new()
    }
}

impl Crc32Fold {
    pub const fn new() -> Self {
        Self::new_with_initial(CRC32_INITIAL_VALUE)
    }

    pub const fn new_with_initial(initial: u32) -> Self {
        Self {
            #[cfg(target_arch = "x86_64")]
            fold: pclmulqdq::Accumulator::new(),
            value: initial,
        }
    }

    #[cfg(all(target_arch = "x86_64", feature = "std"))]
    pub(crate) fn is_pclmulqdq_enabled() -> bool {
        std::is_x86_feature_detected!("pclmulqdq")
            && std::is_x86_feature_detected!("sse2")
            && std::is_x86_feature_detected!("sse4.1")
    }

    #[cfg(all(target_arch = "aarch64", feature = "std"))]
    pub(crate) fn is_crc_enabled() -> bool {
        std::arch::is_aarch64_feature_detected!("crc")
    }

    pub fn fold(&mut self, src: &[u8], _start: u32) {
        #[cfg(all(target_arch = "x86_64", feature = "std"))]
        if Self::is_pclmulqdq_enabled() {
            return self.fold.fold(src, _start);
        }

        #[cfg(all(target_arch = "aarch64", feature = "std"))]
        if Self::is_crc_enabled() {
            self.value = self::acle::crc32_acle_aarch64(self.value, src);
            return;
        }

        // in this case the start value is ignored
        self.value = braid::crc32_braid::<5>(self.value, src);
    }

    pub fn fold_copy(&mut self, dst: &mut [MaybeUninit<u8>], src: &[u8]) {
        #[cfg(all(target_arch = "x86_64", feature = "std"))]
        if Self::is_pclmulqdq_enabled() {
            return self.fold.fold_copy(dst, src);
        }

        self.fold(src, 0);
        dst[..src.len()].copy_from_slice(slice_to_uninit(src));
    }

    pub fn finish(self) -> u32 {
        #[cfg(all(target_arch = "x86_64", feature = "std"))]
        if Self::is_pclmulqdq_enabled() {
            return unsafe { self.fold.finish() };
        }

        self.value
    }
}

// when stable, use MaybeUninit::write_slice
fn slice_to_uninit(slice: &[u8]) -> &[MaybeUninit<u8>] {
    // safety: &[T] and &[MaybeUninit<T>] have the same layout
    unsafe { &*(slice as *const [u8] as *const [MaybeUninit<u8>]) }
}

#[cfg(test)]
mod test {
    use test::braid::crc32_braid;

    use super::*;

    const INPUT: [u8; 1024] = {
        let mut array = [0; 1024];
        let mut i = 0;
        while i < array.len() {
            array[i] = i as u8;
            i += 1;
        }

        array
    };

    #[test]
    fn test_crc32_fold() {
        // input large enough to trigger the SIMD
        let mut h = crc32fast::Hasher::new_with_initial(CRC32_INITIAL_VALUE);
        h.update(&INPUT);
        assert_eq!(crc32(CRC32_INITIAL_VALUE, &INPUT), h.finalize());
    }

    #[test]
    fn test_crc32_fold_align() {
        // SIMD algorithm is sensitive to alignment;
        for i in 0..16 {
            for start in [CRC32_INITIAL_VALUE, 42] {
                let mut h = crc32fast::Hasher::new_with_initial(start);
                h.update(&INPUT[i..]);
                assert_eq!(
                    crc32(start, &INPUT[i..]),
                    h.finalize(),
                    "offset = {i}, start = {start}"
                );
            }
        }
    }

    #[test]
    fn test_crc32_fold_copy() {
        // input large enough to trigger the SIMD
        let mut h = crc32fast::Hasher::new_with_initial(CRC32_INITIAL_VALUE);
        h.update(&INPUT);
        let mut dst = [0; INPUT.len()];
        let mut dst = ReadBuf::new(&mut dst);

        assert_eq!(crc32_copy(&mut dst, &INPUT), h.finalize());

        assert_eq!(INPUT, dst.filled());
    }

    quickcheck::quickcheck! {
        fn crc_fold_is_crc32fast(v: Vec<u8>, start: u32) -> bool {
            let mut h = crc32fast::Hasher::new_with_initial(start);
            h.update(&v);

            let a = crc32(start, &v) ;
            let b = h.finalize();

            a == b
        }

        fn crc_fold_copy_is_crc32fast(v: Vec<u8>) -> bool {
            let mut h = crc32fast::Hasher::new_with_initial(CRC32_INITIAL_VALUE);
            h.update(&v);

            let mut dst = vec![0; v.len()];
            let mut dst = ReadBuf::new(&mut dst);

            let a = crc32_copy(&mut dst, &v) ;
            let b = h.finalize();

            assert_eq!(a,b);

            v == dst.filled()
        }
    }

    #[test]
    fn chunked() {
        const INPUT: &[&[u8]] = &[
            &[116],
            &[111, 107, 105, 111, 44, 32, 97, 115],
            &[121, 110, 99, 45, 115, 116, 100, 44],
            &[32, 97, 110, 100, 32, 115, 109, 111],
            &[108, 46, 32, 89, 111, 117, 226, 128],
            &[153, 118, 101, 32, 112, 114, 111, 98],
            &[97, 98, 108, 121, 32, 117, 115, 101],
            &[100, 32, 116, 104, 101, 109, 32, 97],
            &[116, 32, 115, 111, 109, 101, 32, 112],
            &[111, 105, 110, 116, 44, 32, 101, 105],
            &[116, 104, 101, 114, 32, 100, 105, 114],
            &[101, 99, 116, 108, 121, 32, 111, 114],
            &[0],
        ];

        const START: u32 = 2380683574;

        let mut in_chunks = START;
        for chunk in INPUT {
            in_chunks = crc32(in_chunks, chunk);
        }

        let flattened: Vec<_> = INPUT.iter().copied().flatten().copied().collect();
        let flat = crc32(START, &flattened);

        assert_eq!(in_chunks, flat);
    }

    #[test]
    fn nasty_alignment() {
        const START: u32 = 2380683574;

        const FLAT: &[u8] = &[
            116, 111, 107, 105, 111, 44, 32, 97, 115, 121, 110, 99, 45, 115, 116, 100, 44, 32, 97,
            110, 100, 32, 115, 109, 111, 108, 46, 32, 89, 111, 117, 226, 128, 153, 118, 101, 32,
            112, 114, 111, 98, 97, 98, 108, 121, 32, 117, 115, 101, 100, 32, 116, 104, 101, 109,
            32, 97, 116, 32, 115, 111, 109, 101, 32, 112, 111, 105, 110, 116, 44, 32, 101, 105,
            116, 104, 101, 114, 32, 100, 105, 114, 101, 99, 116, 108, 121, 32, 111, 114, 0,
        ];

        let mut i = 0;
        let mut flat = FLAT.to_vec();
        while flat[i..].as_ptr() as usize % 16 != 15 {
            flat.insert(0, 0);
            i += 1;
        }

        let flat = &flat[i..];

        assert_eq!(crc32_braid::<5>(START, flat), crc32(START, flat));
        assert_eq!(crc32(2380683574, flat), 1175758345);
    }
}
