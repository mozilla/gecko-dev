use crate::error::Error;

pub trait UnwindRule: Copy + core::fmt::Debug {
    type UnwindRegs;

    fn exec<F>(
        self,
        is_first_frame: bool,
        regs: &mut Self::UnwindRegs,
        read_stack: &mut F,
    ) -> Result<Option<u64>, Error>
    where
        F: FnMut(u64) -> Result<u64, ()>;

    fn rule_for_stub_functions() -> Self;
    fn rule_for_function_start() -> Self;
    fn fallback_rule() -> Self;
}
