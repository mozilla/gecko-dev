use std::fmt::Debug;
use zerocopy_derive::{FromBytes, FromZeroes, Unaligned};

use super::unaligned::{U16, U32};

// Written with help from https://gankra.github.io/blah/compact-unwinding/

/// The `__unwind_info` header.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct CompactUnwindInfoHeader {
    /// The version. Only version 1 is currently defined
    pub version: U32,

    /// The array of U32 global opcodes (offset relative to start of root page).
    ///
    /// These may be indexed by "compressed" second-level pages.
    pub global_opcodes_offset: U32,
    pub global_opcodes_len: U32,

    /// The array of U32 global personality codes (offset relative to start of root page).
    ///
    /// Personalities define the style of unwinding that an unwinder should use,
    /// and how to interpret the LSDA functions for a function (see below).
    pub personalities_offset: U32,
    pub personalities_len: U32,

    /// The array of [`PageEntry`]'s describing the second-level pages
    /// (offset relative to start of root page).
    pub pages_offset: U32,
    pub pages_len: U32,
    // After this point there are several dynamically-sized arrays whose precise
    // order and positioning don't matter, because they are all accessed using
    // offsets like the ones above. The arrays are:

    // global_opcodes: [u32; global_opcodes_len],
    // personalities: [u32; personalities_len],
    // pages: [PageEntry; pages_len],
    // lsdas: [LsdaEntry; unknown_len],
}

/// One element of the array of pages.
#[derive(Unaligned, FromZeroes, FromBytes, Clone, Copy)]
#[repr(C)]
pub struct PageEntry {
    /// The first address mapped by this page.
    ///
    /// This is useful for binary-searching for the page that can map
    /// a specific address in the binary (the primary kind of lookup
    /// performed by an unwinder).
    pub first_address: U32,

    /// Offset of the second-level page.
    ///
    /// This may point to either a [`RegularPage`] or a [`CompressedPage`].
    /// Which it is can be determined by the 32-bit "kind" value that is at
    /// the start of both layouts.
    pub page_offset: U32,

    /// Base offset into the lsdas array that functions in this page will be
    /// relative to.
    pub lsda_index_offset: U32,
}

/// A non-compressed page.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct RegularPage {
    /// Always 2 (use to distinguish from CompressedPage).
    pub kind: U32,

    /// The Array of [`RegularFunctionEntry`]'s (offset relative to **start of this page**).
    pub functions_offset: U16,
    pub functions_len: U16,
}

/// A "compressed" page.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct CompressedPage {
    /// Always 3 (use to distinguish from RegularPage).
    pub kind: U32,

    /// The array of compressed u32 function entries (offset relative to **start of this page**).
    ///
    /// Entries are a u32 that contains two packed values (from highest to lowest bits):
    /// * 8 bits: opcode index
    ///   * 0..global_opcodes_len => index into global palette
    ///   * global_opcodes_len..255 => index into local palette (subtract global_opcodes_len)
    /// * 24 bits: instruction address
    ///   * address is relative to this page's first_address!
    pub functions_offset: U16,
    pub functions_len: U16,

    /// The array of u32 local opcodes for this page (offset relative to **start of this page**).
    pub local_opcodes_offset: U16,
    pub local_opcodes_len: U16,
}

/// An opcode.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct Opcode(pub U32);

/// A function entry from a non-compressed page.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct RegularFunctionEntry {
    /// The address in the binary for this function entry (absolute).
    pub address: U32,

    /// The opcode for this address.
    pub opcode: Opcode,
}
