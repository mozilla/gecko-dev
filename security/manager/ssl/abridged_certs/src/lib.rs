/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

mod builtins;
mod cert_storage;
mod tls;

use log;
use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_ERROR_UNEXPECTED, NS_OK};
use std::fmt::Display;
use std::io::Write;
use tls::{CertificateMessage, CompressedCertEntry, UncompressedCertEntry};

#[no_mangle]
pub extern "C" fn certs_are_available() -> bool {
    match cert_storage::has_all_certs_by_hash(builtins::get_needed_hashes()) {
        Ok(result) => {
            log::debug!("certs_are_available {}", result);
            return result;
        }
        Err(e) => {
            log::warn!("certs_are_available failed: {:?}", e);
            return false;
        }
    }
}

#[no_mangle]
pub extern "C" fn decompress(
    input: *const u8,
    input_len: usize,
    output: *mut u8,
    output_len: usize,
    used_len: *mut usize,
) -> nsresult {
    if input.is_null() || output.is_null() || used_len.is_null() {
        return NS_ERROR_INVALID_ARG;
    }
    let input_slice = unsafe { std::slice::from_raw_parts(input, input_len) };

    let mut output = unsafe {
        std::ptr::write_bytes(output, 0, output_len);
        std::slice::from_raw_parts_mut(output, output_len)
    };

    let size = match pass_1_decompression(input_slice, output_len, &mut output) {
        Ok(size) => size,
        Err(e) => {
            log::error!("Error during pass 1 decompression: {}", e);
            return NS_ERROR_UNEXPECTED;
        }
    };

    unsafe {
        *used_len = size;
    }

    log::debug!(
        "successfully decompressed {} input bytes to {} output_bytes ",
        input_len,
        size,
    );
    NS_OK
}

fn pass_1_mapping(mut entry: UncompressedCertEntry) -> Result<CompressedCertEntry, AbridgedError> {
    let id_or_cert = &entry.data;
    let Ok(id) = TryInto::<&[u8; 3]>::try_into(id_or_cert.as_slice()) else {
        // Avoid doing a lookup when we know its not an identifier.
        log::trace!("Passing through directly {:#02X?}", entry.data);
        return Ok(entry);
    };

    log::trace!("Abridged Certs Identifier: {:#02X?}", id);
    let Some(hash) = builtins::id_to_hash(id) else {
        return Err(AbridgedError::UnknownIdentifier(id_or_cert.to_vec()));
    };

    match cert_storage::get_cert_from_hash(&hash) {
        Ok(cert_bytes) => entry.data = cert_bytes.to_vec(),
        Err(err) => {
            return Err(AbridgedError::UnableToLoadCertByHash(hash.to_vec(), err));
        }
    };
    Ok(entry)
}
fn pass_1_decompression(
    input: &[u8],
    expected_len: usize,
    output: &mut impl Write,
) -> Result<usize, AbridgedError> {
    let (mut cert_msg, tail) = CertificateMessage::read_from_bytes(&input)?;

    if !tail.is_empty() {
        // Trailing data on a certificate message is forbidden by Abridged Certs spec
        return Err(AbridgedError::ParsingInvalidCertificateMessage);
    }

    let mut new_entries = Vec::with_capacity(cert_msg.certificate_entries.len());

    // We keep a running tally of the decompressed message's size.
    let mut current_size = cert_msg.get_size();

    for entry in cert_msg.certificate_entries {
        current_size -= entry.get_size();
        let mapping = pass_1_mapping(entry)?;
        current_size += mapping.get_size();
        new_entries.push(mapping);
        if current_size > expected_len {
            return Err(AbridgedError::LargerThanExpectedSize(expected_len));
        }
    }
    cert_msg.certificate_entries = new_entries;
    assert_eq!(current_size, cert_msg.get_size());
    cert_msg.write_to_bytes(output)?;
    Ok(cert_msg.get_size())
}

#[derive(Debug)]
pub enum AbridgedError {
    UnknownError,
    ParsingInvalidTLSVec,
    ParsingInvalidCertificateMessage,
    InvalidOperation,
    LargerThanExpectedSize(usize),
    ReadingError(std::io::Error),
    WritingError(std::io::Error),
    UnableToLoadCertByHash(Vec<u8>, nsresult),
    UnknownIdentifier(Vec<u8>),
}

impl Display for AbridgedError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AbridgedError::UnknownError => write!(f, "Unknown Error"),
            AbridgedError::ParsingInvalidTLSVec => write!(f, "ParsingInvalidTLSVec"),
            AbridgedError::ParsingInvalidCertificateMessage => {
                write!(f, "ParsingInvalidCertificateMessage")
            }
            AbridgedError::InvalidOperation => write!(f, "InvalidOperation"),
            AbridgedError::LargerThanExpectedSize(size) => {
                write!(f, "Larger Than Expected Sizes {}", size)
            }
            AbridgedError::ReadingError(x) => write!(f, "Writing Error {}", x),
            AbridgedError::WritingError(x) => write!(f, "Writing Error {}", x),
            AbridgedError::UnableToLoadCertByHash(hash, error) => write!(
                f,
                "Unable to Load Cert for Hash {:#02X?}. Error: {}",
                hash, error
            ),
            AbridgedError::UnknownIdentifier(id) => {
                write!(f, "Unrecognized Identifier {:#02X?}", id)
            }
        }
    }
}
