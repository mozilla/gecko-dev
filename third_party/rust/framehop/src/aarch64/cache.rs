use super::unwind_rule::*;
use crate::cache::*;

/// The unwinder cache type for [`UnwinderAarch64`](super::UnwinderAarch64).
pub struct CacheAarch64<P: AllocationPolicy = MayAllocateDuringUnwind>(
    pub Cache<UnwindRuleAarch64, P>,
);

impl CacheAarch64<MayAllocateDuringUnwind> {
    /// Create a new cache.
    pub fn new() -> Self {
        Self(Cache::new())
    }
}

impl<P: AllocationPolicy> CacheAarch64<P> {
    /// Create a new cache.
    pub fn new_in() -> Self {
        Self(Cache::new())
    }

    /// Returns a snapshot of the cache usage statistics.
    pub fn stats(&self) -> CacheStats {
        self.0.rule_cache.stats()
    }
}

impl<P: AllocationPolicy> Default for CacheAarch64<P> {
    fn default() -> Self {
        Self::new_in()
    }
}
