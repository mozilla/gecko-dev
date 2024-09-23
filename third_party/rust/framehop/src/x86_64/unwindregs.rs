use core::fmt::Debug;

use crate::display_utils::HexNum;

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct UnwindRegsX86_64 {
    ip: u64,
    regs: [u64; 16],
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u8)]
pub enum Reg {
    RAX,
    RDX,
    RCX,
    RBX,
    RSI,
    RDI,
    RBP,
    RSP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
}

impl UnwindRegsX86_64 {
    pub fn new(ip: u64, sp: u64, bp: u64) -> Self {
        let mut r = Self {
            ip,
            regs: Default::default(),
        };
        r.set_sp(sp);
        r.set_bp(bp);
        r
    }

    #[inline(always)]
    pub fn get(&self, reg: Reg) -> u64 {
        self.regs[reg as usize]
    }
    #[inline(always)]
    pub fn set(&mut self, reg: Reg, value: u64) {
        self.regs[reg as usize] = value;
    }

    #[inline(always)]
    pub fn ip(&self) -> u64 {
        self.ip
    }
    #[inline(always)]
    pub fn set_ip(&mut self, ip: u64) {
        self.ip = ip
    }

    #[inline(always)]
    pub fn sp(&self) -> u64 {
        self.get(Reg::RSP)
    }
    #[inline(always)]
    pub fn set_sp(&mut self, sp: u64) {
        self.set(Reg::RSP, sp)
    }

    #[inline(always)]
    pub fn bp(&self) -> u64 {
        self.get(Reg::RBP)
    }
    #[inline(always)]
    pub fn set_bp(&mut self, bp: u64) {
        self.set(Reg::RBP, bp)
    }
}

impl Debug for UnwindRegsX86_64 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("UnwindRegsX86_64")
            .field("ip", &HexNum(self.ip()))
            .field("rax", &HexNum(self.get(Reg::RAX)))
            .field("rdx", &HexNum(self.get(Reg::RDX)))
            .field("rcx", &HexNum(self.get(Reg::RCX)))
            .field("rbx", &HexNum(self.get(Reg::RBX)))
            .field("rsi", &HexNum(self.get(Reg::RSI)))
            .field("rdi", &HexNum(self.get(Reg::RDI)))
            .field("rbp", &HexNum(self.get(Reg::RBP)))
            .field("rsp", &HexNum(self.get(Reg::RSP)))
            .field("r8", &HexNum(self.get(Reg::R8)))
            .field("r9", &HexNum(self.get(Reg::R9)))
            .field("r10", &HexNum(self.get(Reg::R10)))
            .field("r11", &HexNum(self.get(Reg::R11)))
            .field("r12", &HexNum(self.get(Reg::R12)))
            .field("r13", &HexNum(self.get(Reg::R13)))
            .field("r14", &HexNum(self.get(Reg::R14)))
            .field("r15", &HexNum(self.get(Reg::R15)))
            .finish()
    }
}
