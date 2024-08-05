use std::fmt::Debug;

use zerocopy_derive::{FromBytes, FromZeroes, Unaligned};

/// An unaligned little-endian `u32` value.
#[derive(
    Unaligned, FromZeroes, FromBytes, Default, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
#[repr(transparent)]
pub struct U32([u8; 4]);

impl From<u32> for U32 {
    fn from(n: u32) -> Self {
        U32(n.to_le_bytes())
    }
}

impl From<U32> for u32 {
    fn from(n: U32) -> Self {
        u32::from_le_bytes(n.0)
    }
}

impl Debug for U32 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        u32::fmt(&(*self).into(), f)
    }
}

/// An unaligned little-endian `u16` value.
#[derive(
    Unaligned, FromZeroes, FromBytes, Default, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash,
)]
#[repr(transparent)]
pub struct U16([u8; 2]);

impl From<u16> for U16 {
    fn from(n: u16) -> Self {
        U16(n.to_le_bytes())
    }
}

impl From<U16> for u16 {
    fn from(n: U16) -> Self {
        u16::from_le_bytes(n.0)
    }
}

impl Debug for U16 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        u16::fmt(&(*self).into(), f)
    }
}
