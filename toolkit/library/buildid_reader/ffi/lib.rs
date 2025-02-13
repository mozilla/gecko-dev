/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use buildid_reader::{result::Error, BuildIdReader};
use log::{error, trace};
use nserror::*;
use nsstring::{nsAString, nsCString};
use std::path::Path;

#[no_mangle]
pub extern "C" fn read_toolkit_buildid_from_file(
    fname: &nsAString,
    nname: &nsCString,
    rv_build_id: &mut nsCString,
) -> nsresult {
    let fname_str = fname.to_string();
    let path = Path::new(&fname_str);
    let note_name = nname.to_string();

    trace!("read_toolkit_buildid_from_file {} {}", fname, nname);

    match BuildIdReader::new(&path).and_then(|mut reader| reader.read_string_build_id(&note_name)) {
        Ok(id) => {
            trace!("read_toolkit_buildid_from_file {}", id);
            rv_build_id.assign(&id);
            NS_OK
        }
        Err(err) => {
            error!("read_toolkit_buildid_from_file failed to read string build id from note {:?} with error {:?}", note_name, err);
            match err {
                Error::FailedToOpenFile { .. } => NS_ERROR_FILE_UNRECOGNIZED_PATH,
                Error::FailedToRead { .. } => NS_ERROR_OUT_OF_MEMORY,
                Error::StringFromBytesNulError { .. } | Error::StringFromBytesUtf8Error { .. } => {
                    NS_ERROR_ILLEGAL_VALUE
                }
                Error::Goblin { .. } | Error::NotFatArchive => NS_ERROR_ILLEGAL_VALUE,
                Error::CopyBytes { .. } => NS_ERROR_OUT_OF_MEMORY,
                Error::NoteNotAvailable | Error::ArchNotAvailable => NS_ERROR_NOT_AVAILABLE,
                Error::InvalidNoteName => NS_ERROR_INVALID_ARG,
                Error::NotEnoughData { .. } => NS_ERROR_FAILURE,
            }
        }
    }
}
