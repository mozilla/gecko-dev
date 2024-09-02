#![allow(dead_code)]
#![allow(unreachable_code)]

#[inline(always)]
pub fn is_enabled_sse() -> bool {
    #[cfg(any(target_arch = "x86_64", target_arch = "x86"))]
    #[cfg(feature = "std")]
    return std::is_x86_feature_detected!("sse");

    false
}

#[inline(always)]
pub fn is_enabled_sse42() -> bool {
    #[cfg(any(target_arch = "x86_64", target_arch = "x86"))]
    #[cfg(feature = "std")]
    return std::is_x86_feature_detected!("sse4.2");

    false
}

#[inline(always)]
pub fn is_enabled_avx2() -> bool {
    #[cfg(any(target_arch = "x86_64", target_arch = "x86"))]
    #[cfg(feature = "std")]
    return std::is_x86_feature_detected!("avx2");

    false
}

#[inline(always)]
pub fn is_enabled_avx512() -> bool {
    #[cfg(any(target_arch = "x86_64", target_arch = "x86"))]
    #[cfg(feature = "std")]
    return std::is_x86_feature_detected!("avx512f");

    false
}

#[inline(always)]
pub fn is_enabled_pclmulqdq() -> bool {
    #[cfg(target_arch = "x86_64")]
    #[cfg(feature = "std")]
    return std::is_x86_feature_detected!("pclmulqdq")
        && std::is_x86_feature_detected!("sse2")
        && std::is_x86_feature_detected!("sse4.1");

    false
}

#[inline(always)]
pub fn is_enabled_neon() -> bool {
    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "std")]
    return std::arch::is_aarch64_feature_detected!("neon");

    false
}

#[inline(always)]
pub fn is_enabled_crc() -> bool {
    #[cfg(target_arch = "aarch64")]
    #[cfg(feature = "std")]
    return std::arch::is_aarch64_feature_detected!("crc");

    false
}
