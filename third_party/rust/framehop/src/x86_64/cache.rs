use super::unwind_rule::*;
use crate::cache::*;

/// The unwinder cache type for [`UnwinderX86_64`](super::UnwinderX86_64).
pub struct CacheX86_64<P: AllocationPolicy = MayAllocateDuringUnwind>(
    pub Cache<UnwindRuleX86_64, P>,
);

impl CacheX86_64<MayAllocateDuringUnwind> {
    /// Create a new cache.
    pub fn new() -> Self {
        Self(Cache::new())
    }
}

impl<P: AllocationPolicy> CacheX86_64<P> {
    /// Create a new cache.
    pub fn new_in() -> Self {
        Self(Cache::new())
    }

    /// Returns a snapshot of the cache usage statistics.
    pub fn stats(&self) -> CacheStats {
        self.0.rule_cache.stats()
    }
}

impl<P: AllocationPolicy> Default for CacheX86_64<P> {
    fn default() -> Self {
        Self::new_in()
    }
}
