/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Zip utilities related to localization packaging.

use crate::std::{fs::File, io::Read, path::Path};
use anyhow::{Context, Result};
use zip::read::{ArchiveOffset, Config as ZipConfig, ZipArchive};

/// A firefox archive file.
pub type Archive = ZipArchive<File>;

/// Read a zip file.
pub fn read_zip(path: &Path) -> Result<Archive> {
    ZipArchive::with_config(
        ZipConfig {
            // The archive starts at the beginning of the file (it's a standard zip file, no
            // prefix data).
            //
            // Without this explicit offset, the default behavior will not work for omnijar files
            // (which have the central directory at the beginning of the file, rather than just
            // before the end of central directory maker).
            archive_offset: ArchiveOffset::Known(0),
        },
        File::open(&path).with_context(|| format!("failed to open {}", path.display()))?,
    )
    .with_context(|| format!("failed to read zip archive in {}", path.display()))
}

/// Read a file from the given zip archive as a string.
pub fn read_archive_file_as_string(archive: &mut Archive, path: &str) -> anyhow::Result<String> {
    let mut file = archive
        .by_name(path)
        .with_context(|| format!("failed to locate {path} in archive"))?;
    let mut data = String::new();
    file.read_to_string(&mut data)
        .with_context(|| format!("failed to read {path} from archive"))?;
    Ok(data)
}
