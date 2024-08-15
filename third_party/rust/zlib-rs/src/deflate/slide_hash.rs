pub fn slide_hash(state: &mut crate::deflate::State) {
    let wsize = state.w_size as u16;

    slide_hash_chain(state.head, wsize);
    slide_hash_chain(state.prev, wsize);
}

fn slide_hash_chain(table: &mut [u16], wsize: u16) {
    #[cfg(all(target_arch = "x86_64", feature = "std"))]
    if std::is_x86_feature_detected!("avx2") {
        return avx2::slide_hash_chain(table, wsize);
    }

    #[cfg(all(target_arch = "aarch64", feature = "std"))]
    if std::arch::is_aarch64_feature_detected!("neon") {
        return neon::slide_hash_chain(table, wsize);
    }

    rust::slide_hash_chain(table, wsize);
}

mod rust {
    pub fn slide_hash_chain(table: &mut [u16], wsize: u16) {
        for m in table.iter_mut() {
            *m = m.saturating_sub(wsize);
        }
    }
}

#[cfg(target_arch = "aarch64")]
mod neon {
    use core::arch::aarch64::{
        uint16x8_t, uint16x8x4_t, vdupq_n_u16, vld1q_u16_x4, vqsubq_u16, vst1q_u16_x4,
    };

    pub fn slide_hash_chain(table: &mut [u16], wsize: u16) {
        assert!(std::arch::is_aarch64_feature_detected!("neon"));
        unsafe { slide_hash_chain_internal(table, wsize) }
    }

    #[target_feature(enable = "neon")]
    unsafe fn slide_hash_chain_internal(table: &mut [u16], wsize: u16) {
        debug_assert_eq!(table.len() % 32, 0);

        let v = unsafe { vdupq_n_u16(wsize) };

        for chunk in table.chunks_exact_mut(32) {
            unsafe {
                let p0 = vld1q_u16_x4(chunk.as_ptr());
                let p0 = vqsubq_u16_x4_x1(p0, v);
                vst1q_u16_x4(chunk.as_mut_ptr(), p0);
            }
        }
    }

    #[target_feature(enable = "neon")]
    unsafe fn vqsubq_u16_x4_x1(a: uint16x8x4_t, b: uint16x8_t) -> uint16x8x4_t {
        uint16x8x4_t(
            vqsubq_u16(a.0, b),
            vqsubq_u16(a.1, b),
            vqsubq_u16(a.2, b),
            vqsubq_u16(a.3, b),
        )
    }
}

#[cfg(target_arch = "x86_64")]
mod avx2 {
    use core::arch::x86_64::{
        __m256i, _mm256_loadu_si256, _mm256_set1_epi16, _mm256_storeu_si256, _mm256_subs_epu16,
    };

    pub fn slide_hash_chain(table: &mut [u16], wsize: u16) {
        assert!(std::is_x86_feature_detected!("avx2"));
        unsafe { slide_hash_chain_internal(table, wsize) }
    }

    #[target_feature(enable = "avx2")]
    unsafe fn slide_hash_chain_internal(table: &mut [u16], wsize: u16) {
        debug_assert_eq!(table.len() % 16, 0);

        let ymm_wsize = unsafe { _mm256_set1_epi16(wsize as i16) };

        for chunk in table.chunks_exact_mut(16) {
            let chunk = chunk.as_mut_ptr() as *mut __m256i;

            unsafe {
                let value = _mm256_loadu_si256(chunk);
                let result = _mm256_subs_epu16(value, ymm_wsize);
                _mm256_storeu_si256(chunk, result);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const WSIZE: u16 = 32768;

    const INPUT: [u16; 64] = [
        0, 0, 28790, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 43884, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 64412, 0, 0, 0, 0, 0, 21043, 0, 0, 0, 0, 0, 23707, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 64026, 0, 0, 20182,
    ];

    const OUTPUT: [u16; 64] = [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 31644, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 31258, 0, 0, 0,
    ];

    #[test]
    fn test_slide_hash_rust() {
        let mut input = INPUT;

        rust::slide_hash_chain(&mut input, WSIZE);

        assert_eq!(input, OUTPUT);
    }

    #[test]
    #[cfg(target_arch = "x86_64")]
    fn test_slide_hash_avx2() {
        if std::arch::is_x86_feature_detected!("avx2") {
            let mut input = INPUT;

            avx2::slide_hash_chain(&mut input, WSIZE);

            assert_eq!(input, OUTPUT);
        }
    }

    #[test]
    #[cfg(target_arch = "aarch64")]
    fn test_slide_hash_neon() {
        if std::arch::is_aarch64_feature_detected!("neon") {
            let mut input = INPUT;

            neon::slide_hash_chain(&mut input, WSIZE);

            assert_eq!(input, OUTPUT);
        }
    }

    quickcheck::quickcheck! {
        fn slide_is_rust_slide(v: Vec<u16>, wsize: u16) -> bool {
            // pad to a multiple of 32
            let difference = v.len().next_multiple_of(32) - v.len();
            let mut v = v;
            v.extend(core::iter::repeat(u16::MAX).take(difference));


            let mut a = v.clone();
            let mut b = v;

            rust::slide_hash_chain(&mut a, wsize);
            slide_hash_chain(&mut b, wsize);

            a == b
        }
    }
}
