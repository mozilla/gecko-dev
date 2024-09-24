use super::unwind_rule::UnwindRuleAarch64;
use super::unwindregs::UnwindRegsAarch64;
use crate::arch::Arch;

/// The Aarch64 CPU architecture.
pub struct ArchAarch64;
impl Arch for ArchAarch64 {
    type UnwindRule = UnwindRuleAarch64;
    type UnwindRegs = UnwindRegsAarch64;
}
