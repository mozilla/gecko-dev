use cdsl::isa::{TargetIsa, TargetIsaBuilder};
use cdsl::regs::{RegBankBuilder, RegClassBuilder};

pub fn define() -> TargetIsa {
    let mut isa = TargetIsaBuilder::new("arm64");

    // The `x31` regunit serves as the stack pointer / zero register depending on context. We
    // reserve it and don't model the difference.
    let builder = RegBankBuilder::new("IntRegs", "x")
        .units(32)
        .track_pressure(true);
    let int_regs = isa.add_reg_bank(builder);

    let builder = RegBankBuilder::new("FloatRegs", "v")
        .units(32)
        .track_pressure(true);
    let float_regs = isa.add_reg_bank(builder);

    let builder = RegBankBuilder::new("FlagRegs", "")
        .units(1)
        .names(vec!["nzcv"])
        .track_pressure(false);
    let flag_reg = isa.add_reg_bank(builder);

    let builder = RegClassBuilder::new_toplevel("GPR", int_regs);
    isa.add_reg_class(builder);

    let builder = RegClassBuilder::new_toplevel("FPR", float_regs);
    isa.add_reg_class(builder);

    let builder = RegClassBuilder::new_toplevel("FLAG", flag_reg);
    isa.add_reg_class(builder);

    isa.finish()
}
