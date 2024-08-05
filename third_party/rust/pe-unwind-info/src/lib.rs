//! Zero-copy parsers of the contents of the `.pdata` section and unwind information in PE
//! binaries.
//!
//! On top of these parsers, some higher-level interfaces are provided to easily unwind frames. The
//! parsers and the higher interfaces are written with efficiency in mind, doing minimal copying of
//! data. There is no heap allocation.

//pub mod arm64;
pub mod x86_64;
