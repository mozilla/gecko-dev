use core::num::NonZeroU64;

/// An absolute code address for a stack frame. Can either be taken directly from the
/// instruction pointer ("program counter"), or from a return address.
///
/// These addresses are "AVMAs", i.e. Actual Virtual Memory Addresses, i.e. addresses
/// in the virtual memory of the profiled process.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FrameAddress {
    /// This address is the instruction pointer / program counter. This is what unwinding
    /// starts with.
    InstructionPointer(u64),

    /// This is a return address, i.e. the address to which the CPU will jump to when
    /// returning from a function. This is the address of the instruction *after* the
    /// call instruction.
    ///
    /// Unwinding produces a list of return addresses.
    ReturnAddress(NonZeroU64),
}

impl FrameAddress {
    /// Create a [`FrameAddress::InstructionPointer`].
    pub fn from_instruction_pointer(ip: u64) -> Self {
        FrameAddress::InstructionPointer(ip)
    }

    /// Create a [`FrameAddress::ReturnAddress`]. This returns `None` if the given
    /// address is zero.
    pub fn from_return_address(return_address: u64) -> Option<Self> {
        Some(FrameAddress::ReturnAddress(NonZeroU64::new(
            return_address,
        )?))
    }

    /// The raw address (AVMA).
    pub fn address(self) -> u64 {
        match self {
            FrameAddress::InstructionPointer(address) => address,
            FrameAddress::ReturnAddress(address) => address.into(),
        }
    }

    /// The address (AVMA) that should be used for lookup.
    ///
    /// If this address is taken directly from the instruction pointer, then the lookup
    /// address is just the raw address.
    ///
    /// If this address is a return address, then the lookup address is that address **minus
    /// one byte**. This adjusted address will point inside the call instruction. This
    /// subtraction of one byte is needed if you want to look up unwind information or
    /// debug information, because you usually want the information for the call, not for
    /// the next instruction after the call.
    ///
    /// Furthermore, this distinction matters if a function calls a noreturn function as
    /// the last thing it does: If the call is the final instruction of the function, then
    /// the return address will point *after* the function, into the *next* function.
    /// If, during unwinding, you look up unwind information for that next function, you'd
    /// get incorrect unwinding.
    /// This has been observed in practice with `+[NSThread exit]`.
    pub fn address_for_lookup(self) -> u64 {
        match self {
            FrameAddress::InstructionPointer(address) => address,
            FrameAddress::ReturnAddress(address) => u64::from(address) - 1,
        }
    }

    /// Returns whether this address is a return address.
    pub fn is_return_address(self) -> bool {
        match self {
            FrameAddress::InstructionPointer(_) => false,
            FrameAddress::ReturnAddress(_) => true,
        }
    }
}
