// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(
    clippy::allow_attributes,
    dead_code,
    non_upper_case_globals,
    non_snake_case,
    clippy::cognitive_complexity,
    clippy::too_many_lines,
    reason = "For included bindgen code."
)]

use std::os::raw::{c_uint, c_void};

use crate::{
    constants::Epoch,
    err::{secstatus_to_res, Res},
};

include!(concat!(env!("OUT_DIR"), "/nss_ssl.rs"));
#[expect(non_snake_case, reason = "OK here.")]
mod SSLOption {
    include!(concat!(env!("OUT_DIR"), "/nss_sslopt.rs"));
}

pub enum PRFileDesc {}

// Remap some constants.
#[expect(non_upper_case_globals, reason = "OK here.")]
pub const SECSuccess: SECStatus = _SECStatus_SECSuccess;
#[expect(non_upper_case_globals, reason = "OK here.")]
pub const SECFailure: SECStatus = _SECStatus_SECFailure;

#[derive(Debug, Copy, Clone)]
#[repr(u32)]
pub enum Opt {
    Locking = SSLOption::SSL_NO_LOCKS,
    Tickets = SSLOption::SSL_ENABLE_SESSION_TICKETS,
    OcspStapling = SSLOption::SSL_ENABLE_OCSP_STAPLING,
    Alpn = SSLOption::SSL_ENABLE_ALPN,
    ExtendedMasterSecret = SSLOption::SSL_ENABLE_EXTENDED_MASTER_SECRET,
    SignedCertificateTimestamps = SSLOption::SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
    EarlyData = SSLOption::SSL_ENABLE_0RTT_DATA,
    RecordSizeLimit = SSLOption::SSL_RECORD_SIZE_LIMIT,
    Tls13CompatMode = SSLOption::SSL_ENABLE_TLS13_COMPAT_MODE,
    HelloDowngradeCheck = SSLOption::SSL_ENABLE_HELLO_DOWNGRADE_CHECK,
    SuppressEndOfEarlyData = SSLOption::SSL_SUPPRESS_END_OF_EARLY_DATA,
    Grease = SSLOption::SSL_ENABLE_GREASE,
    EnableChExtensionPermutation = SSLOption::SSL_ENABLE_CH_EXTENSION_PERMUTATION,
}

impl Opt {
    #[must_use]
    pub const fn as_int(self) -> PRInt32 {
        self as PRInt32
    }

    // Some options are backwards, like SSL_NO_LOCKS, so use this to manage that.
    fn map_enabled(self, enabled: bool) -> PRIntn {
        let v = match self {
            Self::Locking => !enabled,
            _ => enabled,
        };
        PRIntn::from(v)
    }

    pub(crate) fn set(self, fd: *mut PRFileDesc, value: bool) -> Res<()> {
        secstatus_to_res(unsafe { SSL_OptionSet(fd, self.as_int(), self.map_enabled(value)) })
    }
}

experimental_api!(SSL_HelloRetryRequestCallback(
    fd: *mut PRFileDesc,
    cb: SSLHelloRetryRequestCallback,
    arg: *mut c_void,
));
experimental_api!(SSL_RecordLayerWriteCallback(
    fd: *mut PRFileDesc,
    cb: SSLRecordWriteCallback,
    arg: *mut c_void,
));
experimental_api!(SSL_RecordLayerData(
    fd: *mut PRFileDesc,
    epoch: Epoch,
    ct: SSLContentType::Type,
    data: *const u8,
    len: c_uint,
));
experimental_api!(SSL_SendSessionTicket(
    fd: *mut PRFileDesc,
    extra: *const u8,
    len: c_uint,
));
experimental_api!(SSL_SetMaxEarlyDataSize(fd: *mut PRFileDesc, size: u32));
experimental_api!(SSL_SetResumptionToken(
    fd: *mut PRFileDesc,
    token: *const u8,
    len: c_uint,
));
experimental_api!(SSL_SetResumptionTokenCallback(
    fd: *mut PRFileDesc,
    cb: SSLResumptionTokenCallback,
    arg: *mut c_void,
));

experimental_api!(SSL_GetResumptionTokenInfo(
    token: *const u8,
    token_len: c_uint,
    info: *mut SSLResumptionTokenInfo,
    len: c_uint,
));

experimental_api!(SSL_DestroyResumptionTokenInfo(
    info: *mut SSLResumptionTokenInfo,
));

#[cfg(test)]
mod tests {
    use super::{SSL_GetNumImplementedCiphers, SSL_NumImplementedCiphers};

    #[test]
    fn num_ciphers() {
        assert!(unsafe { SSL_NumImplementedCiphers } > 0);
        assert!(unsafe { SSL_GetNumImplementedCiphers() } > 0);
        assert_eq!(unsafe { SSL_NumImplementedCiphers }, unsafe {
            SSL_GetNumImplementedCiphers()
        });
    }
}
