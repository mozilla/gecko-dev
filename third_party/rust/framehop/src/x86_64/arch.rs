use super::unwind_rule::UnwindRuleX86_64;
use super::unwindregs::UnwindRegsX86_64;
use crate::arch::Arch;

/// The x86_64 CPU architecture.
pub struct ArchX86_64;
impl Arch for ArchX86_64 {
    type UnwindRule = UnwindRuleX86_64;
    type UnwindRegs = UnwindRegsX86_64;
}
