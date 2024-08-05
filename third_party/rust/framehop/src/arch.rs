use crate::unwind_rule::UnwindRule;

pub trait Arch {
    type UnwindRegs;
    type UnwindRule: UnwindRule<UnwindRegs = Self::UnwindRegs>;
}
