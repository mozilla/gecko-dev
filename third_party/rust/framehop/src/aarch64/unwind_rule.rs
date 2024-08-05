use super::unwindregs::UnwindRegsAarch64;
use crate::add_signed::checked_add_signed;
use crate::error::Error;

use crate::unwind_rule::UnwindRule;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnwindRuleAarch64 {
    /// (sp, fp, lr) = (sp, fp, lr)
    /// Only possible for the first frame. Subsequent frames must get the
    /// return address from somewhere other than the lr register to avoid
    /// infinite loops.
    NoOp,
    /// (sp, fp, lr) = if is_first_frame (sp, fp, lr) else (fp + 16, *fp, *(fp + 8))
    /// Used as a fallback rule.
    NoOpIfFirstFrameOtherwiseFp,
    /// (sp, fp, lr) = (sp + 16x, fp, lr)
    /// Only possible for the first frame. Subsequent frames must get the
    /// return address from somewhere other than the lr register to avoid
    /// infinite loops.
    OffsetSp { sp_offset_by_16: u16 },
    /// (sp, fp, lr) = (sp + 16x, fp, lr) if is_first_frame
    /// This rule reflects an ambiguity in DWARF CFI information. When the
    /// return address is "undefined" because it was omitted, it could mean
    /// "same value", but this is only allowed for the first frame.
    OffsetSpIfFirstFrameOtherwiseStackEndsHere { sp_offset_by_16: u16 },
    /// (sp, fp, lr) = (sp + 16x, fp, *(sp + 8y))
    OffsetSpAndRestoreLr {
        sp_offset_by_16: u16,
        lr_storage_offset_from_sp_by_8: i16,
    },
    /// (sp, fp, lr) = (sp + 16x, *(sp + 8y), *(sp + 8z))
    OffsetSpAndRestoreFpAndLr {
        sp_offset_by_16: u16,
        fp_storage_offset_from_sp_by_8: i16,
        lr_storage_offset_from_sp_by_8: i16,
    },
    /// (sp, fp, lr) = (fp + 16, *fp, *(fp + 8))
    UseFramePointer,
    /// (sp, fp, lr) = (fp + 8x, *(fp + 8y), *(fp + 8z))
    UseFramepointerWithOffsets {
        sp_offset_from_fp_by_8: u16,
        fp_storage_offset_from_fp_by_8: i16,
        lr_storage_offset_from_fp_by_8: i16,
    },
}

impl UnwindRule for UnwindRuleAarch64 {
    type UnwindRegs = UnwindRegsAarch64;

    fn rule_for_stub_functions() -> Self {
        UnwindRuleAarch64::NoOp
    }
    fn rule_for_function_start() -> Self {
        UnwindRuleAarch64::NoOp
    }
    fn fallback_rule() -> Self {
        UnwindRuleAarch64::UseFramePointer
    }

    fn exec<F>(
        self,
        is_first_frame: bool,
        regs: &mut UnwindRegsAarch64,
        read_stack: &mut F,
    ) -> Result<Option<u64>, Error>
    where
        F: FnMut(u64) -> Result<u64, ()>,
    {
        let lr = regs.lr();
        let sp = regs.sp();
        let fp = regs.fp();

        let (new_lr, new_sp, new_fp) = match self {
            UnwindRuleAarch64::NoOp => {
                if !is_first_frame {
                    return Err(Error::DidNotAdvance);
                }
                (lr, sp, fp)
            }
            UnwindRuleAarch64::NoOpIfFirstFrameOtherwiseFp => {
                if is_first_frame {
                    (lr, sp, fp)
                } else {
                    let fp = regs.fp();
                    let new_sp = fp.checked_add(16).ok_or(Error::IntegerOverflow)?;
                    let new_lr =
                        read_stack(fp + 8).map_err(|_| Error::CouldNotReadStack(fp + 8))?;
                    let new_fp = read_stack(fp).map_err(|_| Error::CouldNotReadStack(fp))?;
                    if new_sp <= sp {
                        return Err(Error::FramepointerUnwindingMovedBackwards);
                    }
                    (new_lr, new_sp, new_fp)
                }
            }
            UnwindRuleAarch64::OffsetSpIfFirstFrameOtherwiseStackEndsHere { sp_offset_by_16 } => {
                if !is_first_frame {
                    return Ok(None);
                }
                let sp_offset = u64::from(sp_offset_by_16) * 16;
                let new_sp = sp.checked_add(sp_offset).ok_or(Error::IntegerOverflow)?;
                (lr, new_sp, fp)
            }
            UnwindRuleAarch64::OffsetSp { sp_offset_by_16 } => {
                if !is_first_frame {
                    return Err(Error::DidNotAdvance);
                }
                let sp_offset = u64::from(sp_offset_by_16) * 16;
                let new_sp = sp.checked_add(sp_offset).ok_or(Error::IntegerOverflow)?;
                (lr, new_sp, fp)
            }
            UnwindRuleAarch64::OffsetSpAndRestoreLr {
                sp_offset_by_16,
                lr_storage_offset_from_sp_by_8,
            } => {
                let sp_offset = u64::from(sp_offset_by_16) * 16;
                let new_sp = sp.checked_add(sp_offset).ok_or(Error::IntegerOverflow)?;
                let lr_storage_offset = i64::from(lr_storage_offset_from_sp_by_8) * 8;
                let lr_location =
                    checked_add_signed(sp, lr_storage_offset).ok_or(Error::IntegerOverflow)?;
                let new_lr =
                    read_stack(lr_location).map_err(|_| Error::CouldNotReadStack(lr_location))?;
                (new_lr, new_sp, fp)
            }
            UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                sp_offset_by_16,
                fp_storage_offset_from_sp_by_8,
                lr_storage_offset_from_sp_by_8,
            } => {
                let sp_offset = u64::from(sp_offset_by_16) * 16;
                let new_sp = sp.checked_add(sp_offset).ok_or(Error::IntegerOverflow)?;
                let lr_storage_offset = i64::from(lr_storage_offset_from_sp_by_8) * 8;
                let lr_location =
                    checked_add_signed(sp, lr_storage_offset).ok_or(Error::IntegerOverflow)?;
                let new_lr =
                    read_stack(lr_location).map_err(|_| Error::CouldNotReadStack(lr_location))?;
                let fp_storage_offset = i64::from(fp_storage_offset_from_sp_by_8) * 8;
                let fp_location =
                    checked_add_signed(sp, fp_storage_offset).ok_or(Error::IntegerOverflow)?;
                let new_fp =
                    read_stack(fp_location).map_err(|_| Error::CouldNotReadStack(fp_location))?;
                (new_lr, new_sp, new_fp)
            }
            UnwindRuleAarch64::UseFramePointer => {
                // Do a frame pointer stack walk. Frame-based aarch64 functions store the caller's fp and lr
                // on the stack and then set fp to the address where the caller's fp is stored.
                //
                // Function prologue example (this one also stores x19, x20, x21 and x22):
                // stp  x22, x21, [sp, #-0x30]! ; subtracts 0x30 from sp, and then stores (x22, x21) at sp
                // stp  x20, x19, [sp, #0x10]   ; stores (x20, x19) at sp + 0x10 (== original sp - 0x20)
                // stp  fp, lr, [sp, #0x20]     ; stores (fp, lr) at sp + 0x20 (== original sp - 0x10)
                // add  fp, sp, #0x20           ; sets fp to the address where the old fp is stored on the stack
                //
                // Function epilogue:
                // ldp  fp, lr, [sp, #0x20]     ; restores fp and lr from the stack
                // ldp  x20, x19, [sp, #0x10]   ; restores x20 and x19
                // ldp  x22, x21, [sp], #0x30   ; restores x22 and x21, and then adds 0x30 to sp
                // ret                          ; follows lr to jump back to the caller
                //
                // Functions are called with bl ("branch with link"); bl puts the return address into the lr register.
                // When a function reaches its end, ret reads the return address from lr and jumps to it.
                // On aarch64, the stack pointer is always aligned to 16 bytes, and registers are usually written
                // to and read from the stack in pairs.
                // In frame-based functions, fp and lr are placed next to each other on the stack.
                // So when a function is called, we have the following stack layout:
                //
                //                                                                      [... rest of the stack]
                //                                                                      ^ sp           ^ fp
                //     bl some_function          ; jumps to the function and sets lr = return address
                //                                                                      [... rest of the stack]
                //                                                                      ^ sp           ^ fp
                //     adjust stack ptr, write some registers, and write fp and lr
                //       [more saved regs]  [caller's frame pointer]  [return address]  [... rest of the stack]
                //       ^ sp                                                                          ^ fp
                //     add    fp, sp, #0x20      ; sets fp to where the caller's fp is now stored
                //       [more saved regs]  [caller's frame pointer]  [return address]  [... rest of the stack]
                //       ^ sp               ^ fp
                //     <function contents>       ; can execute bl and overwrite lr with a new value
                //  ...  [more saved regs]  [caller's frame pointer]  [return address]  [... rest of the stack]
                //  ^ sp                    ^ fp
                //
                // So: *fp is the caller's frame pointer, and *(fp + 8) is the return address.
                let fp = regs.fp();
                let new_sp = fp.checked_add(16).ok_or(Error::IntegerOverflow)?;
                let new_lr = read_stack(fp + 8).map_err(|_| Error::CouldNotReadStack(fp + 8))?;
                let new_fp = read_stack(fp).map_err(|_| Error::CouldNotReadStack(fp))?;
                if new_fp == 0 {
                    return Ok(None);
                }
                if new_fp <= fp || new_sp <= sp {
                    return Err(Error::FramepointerUnwindingMovedBackwards);
                }
                (new_lr, new_sp, new_fp)
            }
            UnwindRuleAarch64::UseFramepointerWithOffsets {
                sp_offset_from_fp_by_8,
                fp_storage_offset_from_fp_by_8,
                lr_storage_offset_from_fp_by_8,
            } => {
                let sp_offset_from_fp = u64::from(sp_offset_from_fp_by_8) * 8;
                let new_sp = fp
                    .checked_add(sp_offset_from_fp)
                    .ok_or(Error::IntegerOverflow)?;
                let lr_storage_offset = i64::from(lr_storage_offset_from_fp_by_8) * 8;
                let lr_location =
                    checked_add_signed(fp, lr_storage_offset).ok_or(Error::IntegerOverflow)?;
                let new_lr =
                    read_stack(lr_location).map_err(|_| Error::CouldNotReadStack(lr_location))?;
                let fp_storage_offset = i64::from(fp_storage_offset_from_fp_by_8) * 8;
                let fp_location =
                    checked_add_signed(fp, fp_storage_offset).ok_or(Error::IntegerOverflow)?;
                let new_fp =
                    read_stack(fp_location).map_err(|_| Error::CouldNotReadStack(fp_location))?;

                if new_fp == 0 {
                    return Ok(None);
                }
                if new_fp <= fp || new_sp <= sp {
                    return Err(Error::FramepointerUnwindingMovedBackwards);
                }
                (new_lr, new_sp, new_fp)
            }
        };
        let return_address = regs.lr_mask().strip_ptr_auth(new_lr);
        if return_address == 0 {
            return Ok(None);
        }
        if !is_first_frame && new_sp == sp {
            return Err(Error::DidNotAdvance);
        }
        regs.set_lr(new_lr);
        regs.set_sp(new_sp);
        regs.set_fp(new_fp);

        Ok(Some(return_address))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_basic() {
        let stack = [
            1, 2, 3, 4, 0x40, 0x100200, 5, 6, 0x70, 0x100100, 7, 8, 9, 10, 0x0, 0x0,
        ];
        let mut read_stack = |addr| Ok(stack[(addr / 8) as usize]);
        let mut regs = UnwindRegsAarch64::new(0x100300, 0x10, 0x20);
        let res = UnwindRuleAarch64::NoOp.exec(true, &mut regs, &mut read_stack);
        assert_eq!(res, Ok(Some(0x100300)));
        assert_eq!(regs.sp(), 0x10);
        let res = UnwindRuleAarch64::UseFramePointer.exec(false, &mut regs, &mut read_stack);
        assert_eq!(res, Ok(Some(0x100200)));
        assert_eq!(regs.sp(), 0x30);
        assert_eq!(regs.fp(), 0x40);
        let res = UnwindRuleAarch64::UseFramePointer.exec(false, &mut regs, &mut read_stack);
        assert_eq!(res, Ok(Some(0x100100)));
        assert_eq!(regs.sp(), 0x50);
        assert_eq!(regs.fp(), 0x70);
        let res = UnwindRuleAarch64::UseFramePointer.exec(false, &mut regs, &mut read_stack);
        assert_eq!(res, Ok(None));
    }
}
