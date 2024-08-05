/// The error type used in this crate.
#[derive(thiserror::Error, Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    /// The data slice was not big enough to read the struct, or we
    /// were trying to follow an invalid offset to somewhere outside
    /// of the data bounds.
    #[error("Read error: {0}")]
    ReadError(#[from] ReadError),

    /// Each page has a first_address which is supposed to match the
    /// start address of its first function entry. If the two addresses
    /// don't match, then the lookup will fail for addresses which fall
    /// in the gap between the page start address and the page's first
    /// function's start address.
    #[error("The page entry's first_address didn't match the address of its first function")]
    InvalidPageEntryFirstAddress,

    /// The page kind was set to an unrecognized value.
    #[error("Invalid page kind")]
    InvalidPageKind,

    /// There is only supposed to be one sentinel page, at the very end
    /// of the pages list - its first_address gives the end address of
    /// the unwind info address range. If a sentinel page is encountered
    /// somewhere else, this error is thrown.
    #[error("Unexpected sentinel page")]
    UnexpectedSentinelPage,
}

/// This error indicates that the data slice was not large enough to
/// read the respective item.
#[derive(thiserror::Error, Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReadError {
    #[error("Could not read CompactUnwindInfoHeader")]
    Header,

    #[error("Could not read global opcodes")]
    GlobalOpcodes,

    #[error("Could not read pages")]
    Pages,

    #[error("Could not read RegularPage")]
    RegularPage,

    #[error("Could not read RegularPage functions")]
    RegularPageFunctions,

    #[error("Could not read CompressedPage")]
    CompressedPage,

    #[error("Could not read CompressedPage functions")]
    CompressedPageFunctions,

    #[error("Could not read local opcodes")]
    LocalOpcodes,

    #[error("Could not read page kind")]
    PageKind,
}
