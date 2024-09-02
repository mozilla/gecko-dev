#[warn(unsafe_op_in_unsafe_fn)]
#[cfg(test)]
const MAX_COMPARE_SIZE: usize = 256;

pub fn compare256_slice(src0: &[u8], src1: &[u8]) -> usize {
    let src0 = first_chunk::<_, 256>(src0).unwrap();
    let src1 = first_chunk::<_, 256>(src1).unwrap();

    compare256(src0, src1)
}

fn compare256(src0: &[u8; 256], src1: &[u8; 256]) -> usize {
    #[cfg(target_arch = "x86_64")]
    if crate::cpu_features::is_enabled_avx2() {
        return unsafe { avx2::compare256(src0, src1) };
    }

    #[cfg(target_arch = "aarch64")]
    if crate::cpu_features::is_enabled_neon() {
        return unsafe { neon::compare256(src0, src1) };
    }

    rust::compare256(src0, src1)
}

pub fn compare256_rle_slice(byte: u8, src: &[u8]) -> usize {
    rust::compare256_rle(byte, src)
}

#[inline]
pub const fn first_chunk<T, const N: usize>(slice: &[T]) -> Option<&[T; N]> {
    if slice.len() < N {
        None
    } else {
        // SAFETY: We explicitly check for the correct number of elements,
        //   and do not let the reference outlive the slice.
        Some(unsafe { &*(slice.as_ptr() as *const [T; N]) })
    }
}

mod rust {

    pub fn compare256(src0: &[u8; 256], src1: &[u8; 256]) -> usize {
        // only unrolls 4 iterations; zlib-ng unrolls 8
        src0.iter().zip(src1).take_while(|(x, y)| x == y).count()
    }

    // run-length encoding
    pub fn compare256_rle(byte: u8, src: &[u8]) -> usize {
        assert!(src.len() >= 256, "too short {}", src.len());

        let mut sv = byte as u64;
        sv |= sv << 8;
        sv |= sv << 16;
        sv |= sv << 32;

        let mut len = 0;

        // this optimizes well because we statically limit the slice to 256 bytes.
        // the loop gets unrolled 4 times automatically.
        for chunk in src[..256].chunks_exact(8) {
            let mv = u64::from_le_bytes(chunk.try_into().unwrap());

            let diff = sv ^ mv;

            if diff > 0 {
                let match_byte = diff.trailing_zeros() / 8;
                return len + match_byte as usize;
            }

            len += 8
        }

        256
    }

    #[test]
    fn test_compare256() {
        let str1 = [b'a'; super::MAX_COMPARE_SIZE];
        let mut str2 = [b'a'; super::MAX_COMPARE_SIZE];

        for i in 0..str1.len() {
            str2[i] = 0;

            let match_len = compare256(&str1, &str2);
            assert_eq!(match_len, i);

            str2[i] = b'a';
        }
    }

    #[test]
    fn test_compare256_rle() {
        let mut string = [b'a'; super::MAX_COMPARE_SIZE];

        for i in 0..string.len() {
            string[i] = 0;

            let match_len = compare256_rle(b'a', &string);
            assert_eq!(match_len, i);

            string[i] = b'a';
        }
    }
}

#[cfg(target_arch = "aarch64")]
mod neon {
    use core::arch::aarch64::{
        uint8x16_t, veorq_u8, vgetq_lane_u64, vld1q_u8, vreinterpretq_u64_u8,
    };

    /// # Safety
    ///
    /// Behavior is undefined if the `neon` target feature is not enabled
    #[target_feature(enable = "neon")]
    pub unsafe fn compare256(src0: &[u8; 256], src1: &[u8; 256]) -> usize {
        let src0: &[[u8; 16]; 16] = unsafe { core::mem::transmute(src0) };
        let src1: &[[u8; 16]; 16] = unsafe { core::mem::transmute(src1) };

        let mut len = 0;

        for (a, b) in src0.iter().zip(src1) {
            unsafe {
                let a: uint8x16_t = vld1q_u8(a.as_ptr());
                let b: uint8x16_t = vld1q_u8(b.as_ptr());

                let cmp = veorq_u8(a, b);

                let lane = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 0);
                if lane != 0 {
                    let match_byte = lane.trailing_zeros() / 8;
                    return len + match_byte as usize;
                }

                len += 8;

                let lane = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 1);
                if lane != 0 {
                    let match_byte = lane.trailing_zeros() / 8;
                    return len + match_byte as usize;
                }

                len += 8;
            }
        }

        256
    }

    #[test]
    fn test_compare256() {
        if crate::cpu_features::is_enabled_neon() {
            let str1 = [b'a'; super::MAX_COMPARE_SIZE];
            let mut str2 = [b'a'; super::MAX_COMPARE_SIZE];

            for i in 0..str1.len() {
                str2[i] = 0;

                let match_len = unsafe { compare256(&str1, &str2) };
                assert_eq!(match_len, i);

                str2[i] = b'a';
            }
        }
    }
}

#[cfg(target_arch = "x86_64")]
mod avx2 {
    use core::arch::x86_64::{
        __m256i, _mm256_cmpeq_epi8, _mm256_loadu_si256, _mm256_movemask_epi8,
    };

    /// # Safety
    ///
    /// Behavior is undefined if the `avx` target feature is not enabled
    #[target_feature(enable = "avx2")]
    pub unsafe fn compare256(src0: &[u8; 256], src1: &[u8; 256]) -> usize {
        let src0: &[[u8; 32]; 8] = unsafe { core::mem::transmute(src0) };
        let src1: &[[u8; 32]; 8] = unsafe { core::mem::transmute(src1) };

        let mut len = 0;

        unsafe {
            for (chunk0, chunk1) in src0.iter().zip(src1) {
                let ymm_src0 = _mm256_loadu_si256(chunk0.as_ptr() as *const __m256i);
                let ymm_src1 = _mm256_loadu_si256(chunk1.as_ptr() as *const __m256i);

                let ymm_cmp = _mm256_cmpeq_epi8(ymm_src0, ymm_src1); /* non-identical bytes = 00, identical bytes = FF */
                let mask = _mm256_movemask_epi8(ymm_cmp) as u32;

                if mask != 0xFFFFFFFF {
                    let match_byte = (!mask).trailing_zeros(); /* Invert bits so identical = 0 */
                    return len + match_byte as usize;
                }

                len += 32;
            }
        }

        256
    }

    #[test]
    fn test_compare256() {
        if crate::cpu_features::is_enabled_avx2() {
            let str1 = [b'a'; super::MAX_COMPARE_SIZE];
            let mut str2 = [b'a'; super::MAX_COMPARE_SIZE];

            for i in 0..str1.len() {
                str2[i] = 0;

                let match_len = unsafe { compare256(&str1, &str2) };
                assert_eq!(match_len, i);

                str2[i] = b'a';
            }
        }
    }
}
