/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use super::AbridgedError;
use byteorder::{BigEndian, ReadBytesExt, WriteBytesExt};
use log::trace;
use std::io::Cursor;
use std::io::Write;
use std::u8;

/// Parses a TLS vector as defined in https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
/// The length prefix is specified as WIDTH within 1..4 inclusive.
/// The result either indicates an error or produces the body of the vec
/// and any remaining data.
fn read_tls_vec<'a, const WIDTH: u8>(
    value: &'a [u8],
) -> Result<(&'a [u8], &'a [u8]), AbridgedError> {
    debug_assert!(WIDTH <= 4, "Invalid width specified");

    let Some((len_bytes, remainder)) = value.split_at_checked(usize::from(WIDTH)) else {
        return Err(AbridgedError::ParsingInvalidTLSVec);
    };

    let io_err_wrapper = |x| AbridgedError::ReadingError(x);
    let mut len_rdr = Cursor::new(len_bytes);
    let len: u32 = match WIDTH {
        1 => len_rdr.read_u8().map_err(io_err_wrapper)?.into(),
        2 => len_rdr
            .read_u16::<BigEndian>()
            .map_err(io_err_wrapper)?
            .into(),
        3 => len_rdr.read_u24::<BigEndian>().map_err(io_err_wrapper)?,
        4 => len_rdr.read_u32::<BigEndian>().map_err(io_err_wrapper)?,
        _ => return Err(AbridgedError::InvalidOperation),
    };

    let Some((vec_body, remainder)) = remainder.split_at_checked(len as usize) else {
        return Err(AbridgedError::ParsingInvalidTLSVec);
    };

    trace!(
        "In length: {}, Output length: {}, Remainder Length: {}",
        value.len(),
        vec_body.len(),
        remainder.len()
    );
    Ok((vec_body, remainder))
}

/// Writes out an integer as defined in https://datatracker.ietf.org/doc/html/rfc8446#section-3.3
/// WIDTH must be between 1 and 4
fn write_tls_int<const WIDTH: u8>(writer: &mut impl Write, int: u32) -> Result<(), AbridgedError> {
    debug_assert!(WIDTH <= 4 && WIDTH > 0, "Invalid width specified");
    if u64::from(int) > 2_u64.pow(u32::from(WIDTH) * 8) - 1 {
        return Err(AbridgedError::InvalidOperation);
    }
    // Panics if int is out of range.
    writer
        .write_uint::<BigEndian>(u64::from(int), usize::from(WIDTH))
        .map_err(|x| AbridgedError::WritingError(x))?;
    Ok(())
}

/// Writes out a TLS vector as defined in https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
/// WIDTH must be between 1 and 4
fn write_tls_vec<const WIDTH: u8>(
    value: &[u8],
    writer: &mut impl Write,
) -> Result<(), AbridgedError> {
    debug_assert!(WIDTH <= 4 && WIDTH > 0, "Invalid width specified");

    let len: u32 = value
        .len()
        .try_into()
        .or(Err(AbridgedError::InvalidOperation))?;

    write_tls_int::<WIDTH>(writer, len)?;
    writer
        .write_all(value)
        .or_else(|x| Err(AbridgedError::WritingError(x)))?;
    Ok(())
}

/// These types represent the structure of a TLS 1.3 certificate message
///
/// RFC 8446: 4.4.2
/// enum {
///     X509(0),
///     RawPublicKey(2),
///     (255)
/// } CertificateType;
///
/// struct {
///     select (certificate_type) {
///         case RawPublicKey:
///           /* From RFC 7250 ASN.1_subjectPublicKeyInfo */
///           opaque ASN1_subjectPublicKeyInfo<1..2^24-1>;
///
///         case X509:
///           opaque cert_data<1..2^24-1>;
///     };
///     Extension extensions<0..2^16-1>;
/// } CertificateEntry;
///
/// struct {
///     opaque certificate_request_context<0..2^8-1>;
///     CertificateEntry certificate_list<0..2^24-1>;
/// } Certificate;

#[derive(Debug)]
pub struct CertificateEntry {
    pub data: Vec<u8>,
    pub extensions: Vec<u8>,
}

pub type UncompressedCertEntry = CertificateEntry;
pub type CompressedCertEntry = CertificateEntry;

#[derive(Debug)]
pub struct CertificateMessage {
    pub request_context: Vec<u8>,
    pub certificate_entries: Vec<CertificateEntry>,
}

impl CertificateEntry {
    pub fn read_from_bytes(value: &[u8]) -> Result<(CertificateEntry, &[u8]), AbridgedError> {
        let (data, remainder) = read_tls_vec::<3>(value)?;
        let (extensions, remainder) = read_tls_vec::<2>(remainder)?;
        Ok((
            CertificateEntry {
                data: data.to_vec(),
                extensions: extensions.to_vec(),
            },
            remainder,
        ))
    }

    pub fn write_to_bytes(&self, writer: &mut impl Write) -> Result<(), AbridgedError> {
        write_tls_vec::<3>(&self.data, writer)?;
        write_tls_vec::<2>(&self.extensions, writer)?;
        Ok(())
    }

    pub fn get_size(&self) -> usize {
        let calculated_size = 3 + self.data.len() + 2 + self.extensions.len();

        if cfg!(debug_assertions) {
            let mut output = Vec::with_capacity(calculated_size);
            self.write_to_bytes(&mut output).expect("Shouldn't error");
            debug_assert_eq!(calculated_size, output.len());
        }
        calculated_size
    }
}

impl CertificateMessage {
    pub fn read_from_bytes(value: &[u8]) -> Result<(CertificateMessage, &[u8]), AbridgedError> {
        trace!("Parsing certificate message from {} bytes", value.len());
        let (request_context, certificate_entries) = read_tls_vec::<1>(value)?;
        trace!(
            "Parsing request_context of size {}, {} remaining",
            request_context.len(),
            value.len()
        );
        let (certificate_entries, tail) = read_tls_vec::<3>(certificate_entries)?;
        trace!(
            "Parsing certificate_field of size {}, {} remaining",
            certificate_entries.len(),
            value.len()
        );

        let mut parsed_certificate_entries = Vec::with_capacity(5);

        let mut remaining_data = certificate_entries;
        while !remaining_data.is_empty() {
            let (entry, temp) = CertificateEntry::read_from_bytes(remaining_data)?;
            remaining_data = temp;
            parsed_certificate_entries.push(entry);
        }
        Ok((
            CertificateMessage {
                request_context: request_context.to_vec(),
                certificate_entries: parsed_certificate_entries,
            },
            tail,
        ))
    }

    pub fn write_to_bytes(&self, writer: &mut impl Write) -> Result<(), AbridgedError> {
        let ce_size: u32 = self
            .certificate_entries
            .iter()
            .map(CertificateEntry::get_size)
            .sum::<usize>()
            .try_into()
            .or(Err(AbridgedError::InvalidOperation))?;
        write_tls_vec::<1>(&self.request_context, writer)?;
        write_tls_int::<3>(writer, ce_size)?;
        for ce in &self.certificate_entries {
            ce.write_to_bytes(writer)?;
        }
        Ok(())
    }

    pub fn get_size(&self) -> usize {
        let calculated_size = 1
            + self.request_context.len()
            + 3
            + self
                .certificate_entries
                .iter()
                .map(|x| x.get_size())
                .sum::<usize>();

        if cfg!(debug_assertions) {
            let mut output = Vec::with_capacity(calculated_size);
            self.write_to_bytes(&mut output).expect("Shouldn't error");
            debug_assert_eq!(calculated_size, output.len());
        }
        calculated_size
    }
}

/// These tests do not run in CI because ./mach rusttests does not support crates which link
/// against gecko symbols.
#[cfg(test)]
mod tests {
    use super::CertificateMessage;

    // Borrowed from https://tls13.xargs.org/#server-certificate
    // Added a single byte extension field
    const CERTMSG: &str = "
        0000032b0003253082032130820209a0030201020208155a92adc2048f90300d06092a86
        4886f70d01010b05003022310b300906035504061302555331133011060355040a130a4578616d70
        6c65204341301e170d3138313030353031333831375a170d3139313030353031333831375a302b31
        0b3009060355040613025553311c301a060355040313136578616d706c652e756c666865696d2e6e
        657430820122300d06092a864886f70d01010105000382010f003082010a0282010100c4803606ba
        e7476b089404eca7b691043ff792bc19eefb7d74d7a80d001e7b4b3a4ae60fe8c071fc73e7024c0d
        bcf4bdd11d396bba70464a13e94af83df3e10959547bc955fb412da3765211e1f3dc776caa53376e
        ca3aecbec3aab73b31d56cb6529c8098bcc9e02818e20bf7f8a03afd1704509ece79bd9f39f1ea69
        ec47972e830fb5ca95de95a1e60422d5eebe527954a1e7bf8a86f6466d0d9f16951a4cf7a0469259
        5c1352f2549e5afb4ebfd77a37950144e4c026874c653e407d7d23074401f484ffd08f7a1fa05210
        d1f4f0d5ce79702932e2cabe701fdfad6b4bb71101f44bad666a11130fe2ee829e4d029dc91cdd67
        16dbb9061886edc1ba94210203010001a3523050300e0603551d0f0101ff0404030205a0301d0603
        551d250416301406082b0601050507030206082b06010505070301301f0603551d23041830168014
        894fde5bcc69e252cf3ea300dfb197b81de1c146300d06092a864886f70d01010b05000382010100
        591645a69a2e3779e4f6dd271aba1c0bfd6cd75599b5e7c36e533eff3659084324c9e7a504079d39
        e0d42987ffe3ebdd09c1cf1d914455870b571dd19bdf1d24f8bb9a11fe80fd592ba0398cde11e265
        1e618ce598fa96e5372eef3d248afde17463ebbfabb8e4d1ab502a54ec0064e92f7819660d3f27cf
        209e667fce5ae2e4ac99c7c93818f8b2510722dfed97f32e3e9349d4c66c9ea6396d744462a06b42
        c6d5ba688eac3a017bddfc8e2cfcad27cb69d3ccdca280414465d3ae348ce0f34ab2fb9c61837131
        2b191041641c237f11a5d65c844f0404849938712b959ed685bc5c5dd645ed19909473402926dcb4
        0e3469a15941e8e2cca84bb6084636a00001ff";

    #[test]
    fn happy_path() {
        let mut cert_hex: String = String::from(CERTMSG);
        cert_hex.retain(|x| !x.is_whitespace());
        let mut cert_bytes: Vec<u8> = hex::decode(cert_hex).unwrap().into();
        let _ =
            CertificateMessage::read_from_bytes(&mut cert_bytes).expect("Should correctly decode");
        assert_eq!(cert_bytes.len(), 0, "nothing left over");
    }

    #[test]
    fn round_trip() {
        let mut cert_hex: String = String::from(CERTMSG);
        cert_hex.retain(|x| !x.is_whitespace());
        let cert_bytes: Vec<u8> = hex::decode(cert_hex).unwrap().into();

        let (msg, _) = CertificateMessage::read_from_bytes(&mut cert_bytes.clone())
            .expect("Should correctly decode");

        let msg_bytes: Vec<u8> = Vec::new();
        let mut cursor = std::io::Cursor::new(msg_bytes);
        msg.write_to_bytes(&mut cursor).expect("No errors");

        let msg_bytes: Vec<u8> = cursor.into_inner().into();
        assert_eq!(msg_bytes.len(), cert_bytes.len(), "nothing left over");
        assert_eq!(msg_bytes, cert_bytes);
        assert_eq!(true, false);
    }

    #[test]
    fn large_integers() {
        let msg_bytes: Vec<u8> = Vec::new();
        let mut cursor = std::io::Cursor::new(msg_bytes);
        assert!(super::write_tls_int::<1>(&mut cursor, u8::MAX as u32 + 1,).is_err());
        assert!(super::write_tls_int::<2>(&mut cursor, u16::MAX as u32 + 1).is_err());
        assert!(super::write_tls_int::<3>(&mut cursor, 2_u32.pow(24) + 1).is_err());
        assert!(super::write_tls_int::<4>(&mut cursor, u32::MAX).is_ok());
    }
}
