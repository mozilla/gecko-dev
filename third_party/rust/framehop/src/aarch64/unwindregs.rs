use core::fmt::Debug;

use crate::display_utils::HexNum;

/// The registers used for unwinding on Aarch64. We only need lr (x30), sp (x31),
/// and fp (x29).
///
/// We also have a [`PtrAuthMask`] which allows stripping off the pointer authentication
/// hash bits from the return address when unwinding through libraries which use pointer
/// authentication, e.g. in system libraries on macOS.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct UnwindRegsAarch64 {
    lr_mask: PtrAuthMask,
    lr: u64,
    sp: u64,
    fp: u64,
}

/// Aarch64 CPUs support special instructions which interpret pointers as pair
/// of the pointer address and an encrypted hash: The address is stored in the
/// lower bits and the hash in the high bits. These are called "authenticated"
/// pointers. Special instructions exist to verify pointers before dereferencing
/// them.
///
/// Return address can be such authenticated pointers. To return to an
/// authenticated return address, the "retab" instruction is used instead of
/// the regular "ret" instruction.
///
/// Stack walkers need to strip the encrypted hash from return addresses because
/// they need the raw code address.
///
/// On macOS arm64, system libraries compiled with the arm64e target use pointer
/// pointer authentication for return addresses.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct PtrAuthMask(pub u64);

impl PtrAuthMask {
    /// Create a no-op mask which treats all bits of the pointer as address bits,
    /// so no bits are stripped.
    pub fn new_no_strip() -> Self {
        Self(u64::MAX)
    }

    /// Create a mask for 24 bits hash + 40 bits pointer. This appears to be
    /// what macOS arm64e uses. It is unclear whether we can rely on this or
    /// whether it can change.
    ///
    /// On macOS arm64, this mask can be applied to both authenticated pointers
    /// and to non-authenticated pointers without data loss; non-authenticated
    /// don't appear to use the top 24 bits (they're always zero).
    pub fn new_24_40() -> Self {
        Self(u64::MAX >> 24)
    }

    /// Deduce a mask based on the highest known address. The leading zero bits
    /// in this address will be reserved for the hash.
    pub fn from_max_known_address(address: u64) -> Self {
        Self(u64::MAX >> address.leading_zeros())
    }

    /// Apply the mask to the given pointer.
    #[inline(always)]
    pub fn strip_ptr_auth(&self, ptr: u64) -> u64 {
        ptr & self.0
    }
}

impl UnwindRegsAarch64 {
    /// Create a set of unwind register values and do not apply any pointer
    /// authentication stripping.
    pub fn new(lr: u64, sp: u64, fp: u64) -> Self {
        Self {
            lr_mask: PtrAuthMask::new_no_strip(),
            lr,
            sp,
            fp,
        }
    }

    /// Create a set of unwind register values with the given mask for return
    /// address pointer authentication stripping.
    pub fn new_with_ptr_auth_mask(
        code_ptr_auth_mask: PtrAuthMask,
        lr: u64,
        sp: u64,
        fp: u64,
    ) -> Self {
        Self {
            lr_mask: code_ptr_auth_mask,
            lr: code_ptr_auth_mask.strip_ptr_auth(lr),
            sp,
            fp,
        }
    }

    /// Get the [`PtrAuthMask`] which we apply to the `lr` value.
    #[inline(always)]
    pub fn lr_mask(&self) -> PtrAuthMask {
        self.lr_mask
    }

    /// Get the stack pointer value.
    #[inline(always)]
    pub fn sp(&self) -> u64 {
        self.sp
    }

    /// Set the stack pointer value.
    #[inline(always)]
    pub fn set_sp(&mut self, sp: u64) {
        self.sp = sp
    }

    /// Get the frame pointer value (x29).
    #[inline(always)]
    pub fn fp(&self) -> u64 {
        self.fp
    }

    /// Set the frame pointer value (x29).
    #[inline(always)]
    pub fn set_fp(&mut self, fp: u64) {
        self.fp = fp
    }

    /// Get the lr register value.
    #[inline(always)]
    pub fn lr(&self) -> u64 {
        self.lr
    }

    /// Set the lr register value.
    #[inline(always)]
    pub fn set_lr(&mut self, lr: u64) {
        self.lr = self.lr_mask.strip_ptr_auth(lr)
    }
}

impl Debug for UnwindRegsAarch64 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("UnwindRegsAarch64")
            .field("lr", &HexNum(self.lr))
            .field("sp", &HexNum(self.sp))
            .field("fp", &HexNum(self.fp))
            .finish()
    }
}

#[cfg(test)]
mod test {
    use crate::aarch64::PtrAuthMask;

    #[test]
    fn test() {
        assert_eq!(PtrAuthMask::new_24_40().0, u64::MAX >> 24);
        assert_eq!(PtrAuthMask::new_24_40().0, (1 << 40) - 1);
        assert_eq!(
            PtrAuthMask::from_max_known_address(0x0000aaaab54f7000).0,
            0x0000ffffffffffff
        );
        assert_eq!(
            PtrAuthMask::from_max_known_address(0x0000ffffa3206000).0,
            0x0000ffffffffffff
        );
        assert_eq!(
            PtrAuthMask::from_max_known_address(0xffffffffc05a9000).0,
            0xffffffffffffffff
        );
        assert_eq!(
            PtrAuthMask::from_max_known_address(0x000055ba9f07e000).0,
            0x00007fffffffffff
        );
        assert_eq!(
            PtrAuthMask::from_max_known_address(0x00007f76b8019000).0,
            0x00007fffffffffff
        );
        assert_eq!(
            PtrAuthMask::from_max_known_address(0x000000022a3ccff7).0,
            0x00000003ffffffff
        );
    }
}
