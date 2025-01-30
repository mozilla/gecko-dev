// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::ops::Range;

use neqo_common::{qtrace, Decoder};

/// Finds the range where the SNI extension lives, or returns `None`.
#[must_use]
pub fn find_sni(buf: &[u8]) -> Option<Range<usize>> {
    #[must_use]
    fn skip(dec: &mut Decoder, len: usize) -> Option<()> {
        if len > dec.remaining() {
            return None;
        }
        dec.skip(len);
        Some(())
    }

    #[must_use]
    fn skip_vec<T>(dec: &mut Decoder) -> Option<()>
    where
        T: TryFrom<u64>,
        usize: TryFrom<T>,
    {
        let len = dec.decode_uint::<T>()?;
        skip(dec, usize::try_from(len).ok()?)
    }

    let mut dec = Decoder::from(buf);

    // Return if buf is empty or does not contain a ClientHello (first byte == 1)
    if buf.is_empty() || dec.decode_uint::<u8>()? != 1 {
        return None;
    }
    skip(&mut dec, 3 + 2 + 32)?; // Skip length, version, random
    skip_vec::<u8>(&mut dec)?; // Skip session_id
    skip_vec::<u16>(&mut dec)?; // Skip cipher_suites
    skip_vec::<u8>(&mut dec)?; // Skip compression_methods
    skip(&mut dec, 2)?;

    while dec.remaining() >= 4 {
        let ext_type: u16 = dec.decode_uint()?;
        let ext_len: u16 = dec.decode_uint()?;
        if ext_type == 0 {
            // SNI!
            let sni_len: u16 = dec.decode_uint()?;
            skip(&mut dec, 3)?; // Skip name_type and host_name length
            let start = dec.offset();
            let end = start + usize::from(sni_len) - 3;
            if end > dec.offset() + dec.remaining() {
                return None;
            }
            qtrace!(
                "SNI range {start}..{end}: {:?}",
                String::from_utf8_lossy(&buf[start..end])
            );
            return Some(start..end);
        }
        // Skip extension
        skip(&mut dec, ext_len.into())?;
    }
    None
}

#[cfg(test)]
mod tests {
    const BUF_WITH_SNI: &[u8] = &[
        0x01, // msg_type == 1 (ClientHello)
        0x00, 0x01, 0xfc, // length (arbitrary)
        0x03, 0x03, // version (TLS 1.2)
        0x0e, 0x2d, 0x03, 0x37, 0xd9, 0x14, 0x2b, 0x32, 0x4e, 0xa8, 0xcf, 0x1f, 0xfa, 0x5b, 0x6c,
        0xeb, 0xdd, 0x10, 0xa6, 0x49, 0x6e, 0xbf, 0xe4, 0x32, 0x3d, 0x0c, 0xe4, 0xbf, 0x90, 0xcf,
        0x08, 0x42, // random
        0x00, // session_id length
        0x00, 0x08, // cipher_suites length
        0x13, 0x01, 0x13, 0x03, 0x13, 0x02, 0xca, 0xca, // cipher_suites
        0x01, // compression_methods length
        0x00, // compression_methods
        0x01, 0xcb, // extensions length
        0xff, 0x01, 0x00, 0x01, 0x00, // renegiation_info
        0x00, 0x2d, 0x00, 0x03, 0x02, 0x01, 0x87, // psk_exchange_modes
        // SNI extension
        0x00, 0x00, // Extension type (SNI)
        0x00, 0x0e, // Extension length
        0x00, 0x0c, // Server Name List length
        0x00, // Name type (host_name)
        0x00, 0x09, // Host name length
        0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, // server_name: "localhost"
        0x00, 0x05, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, // status_request
    ];

    #[test]
    fn find_sni() {
        // ClientHello with SNI extension
        let range = super::find_sni(BUF_WITH_SNI).unwrap();
        let expected_range = BUF_WITH_SNI.len() - 18..BUF_WITH_SNI.len() - 9;
        assert_eq!(range, expected_range);
        assert_eq!(&BUF_WITH_SNI[range], b"localhost");
    }

    #[test]
    fn find_sni_no_sni() {
        // ClientHello without SNI extension
        let mut buf = Vec::from(&BUF_WITH_SNI[..BUF_WITH_SNI.len() - 39]);
        let len = buf.len();
        assert!(buf[len - 2] == 0x01 && buf[len - 1] == 0xcb); // Check extensions length
                                                               // Set extensions length to 0
        buf[len - 2] = 0x00;
        buf[len - 1] = 0x00;
        assert!(super::find_sni(&buf).is_none());
    }

    #[test]
    fn find_sni_invalid_sni() {
        // ClientHello with an SNI extension truncated somewhere in the hostname
        let truncated = &BUF_WITH_SNI[..BUF_WITH_SNI.len() - 15];
        assert!(super::find_sni(truncated).is_none());
    }

    #[test]
    fn find_sni_no_ci() {
        // Not a ClientHello (msg_type != 1)
        let buf = [0; 1];
        assert!(super::find_sni(&buf).is_none());
    }

    #[test]
    fn find_sni_malformed_ci() {
        // Buffer starting with `1` but otherwise malformed
        let buf = [1; 1];
        assert!(super::find_sni(&buf).is_none());
    }
}
