#[derive(Debug, Clone)]
pub enum UnwindResult<R> {
    ExecRule(R),
    Uncacheable(u64),
}
