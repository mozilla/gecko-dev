/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ffi::{c_void, OsString};
use std::fs::OpenOptions;
use std::os::windows::{ffi::OsStringExt, fs::OpenOptionsExt, io::AsRawHandle};
use std::path::Path;
use windows_sys::Win32::{
    Foundation::{BOOL, HWND, INVALID_HANDLE_VALUE},
    Security::Cryptography::*,
    Security::WinTrust::*,
    Storage::FileSystem::{
        FILE_FLAG_SEQUENTIAL_SCAN, FILE_SHARE_DELETE, FILE_SHARE_READ, FILE_SHARE_WRITE,
    },
    UI::WindowsAndMessaging::CharUpperBuffW,
};

// Our windows-targets doesn't link wintrust correctly.
#[link(name = "wintrust", kind = "static")]
extern "C" {}

type DWORD = u32;

mod strings;
mod wintrust;

use strings::WideString;

pub fn binary_org_name(binary: &Path) -> Option<String> {
    log::trace!("binary_org_name({})", binary.display());
    let binary_wide = match WideString::new(binary) {
        Err(e) => {
            log::error!("failed to create wide string of binary path: {e}");
            return None;
        }
        Ok(s) => s,
    };

    // Verify trust for the binary and get the certificate context.
    let mut cert_store = CertStore::default();
    let mut crypt_msg = CryptMsg::default();

    let result = unsafe {
        CryptQueryObject(
            CERT_QUERY_OBJECT_FILE,
            binary_wide.pcwstr() as *const _,
            CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
            CERT_QUERY_FORMAT_FLAG_BINARY,
            0,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            &mut *cert_store,
            &mut *crypt_msg,
            std::ptr::null_mut(),
        )
    };
    let verified = if result != 0 {
        // We got a result.
        let mut file_info = WINTRUST_FILE_INFO {
            cbStruct: std::mem::size_of::<WINTRUST_FILE_INFO>()
                .try_into()
                .unwrap(),
            pcwszFilePath: binary_wide.pcwstr(),
            hFile: 0,
            pgKnownSubject: std::ptr::null_mut(),
        };
        verify_trust(|data| {
            data.dwUnionChoice = WTD_CHOICE_FILE;
            data.Anonymous = WINTRUST_DATA_0 {
                pFile: &mut file_info,
            };
        })
    } else {
        log::debug!("checking catalogs for binary org name");
        // We didn't find anything in the binary, so try catalogs.
        let cat_admin = wintrust::CATAdmin::acquire()?;

        log::trace!("acquired CATAdmin");

        // Hash the binary
        let file = match OpenOptions::new()
            .read(true)
            .share_mode(FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE)
            .custom_flags(FILE_FLAG_SEQUENTIAL_SCAN)
            .open(binary)
        {
            Err(e) => {
                log::error!("failed to open file {}: {e}", binary.display());
                return None;
            }
            Ok(v) => v,
        };

        let mut file_hash = cat_admin.calculate_file_hash(&file)?;

        log::trace!("{} hashed to {file_hash:?}", binary.display());

        // Now query the catalog system to see if any catalogs reference a binary with our hash.
        let catalog_info = cat_admin
            .catalog_from_hash(&mut file_hash)
            .and_then(|h| h.get_info())?;

        log::trace!("found catalog info");

        unsafe {
            CryptQueryObject(
                CERT_QUERY_OBJECT_FILE,
                catalog_info.wszCatalogFile.as_ptr() as *const _,
                CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                CERT_QUERY_FORMAT_FLAG_BINARY,
                0,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                &mut *cert_store,
                &mut *crypt_msg,
                std::ptr::null_mut(),
            )
        }
        .into_option()?;

        // WINTRUST_CATALOG_INFO::pcwszMemberTag is commonly set to the string
        // representation of the file hash, so we build that here.
        let mut strlength = DWORD::default();
        let file_hash_to_string = |buf: *mut u16, len: *mut DWORD| {
            unsafe {
                CryptBinaryToStringW(
                    file_hash.as_ptr(),
                    file_hash.len() as DWORD,
                    CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF,
                    buf,
                    len,
                )
            }
            .into_option()
        };
        file_hash_to_string(std::ptr::null_mut(), &mut strlength as *mut _)?;
        let mut file_hash_string = vec![0u16; strlength as usize];
        file_hash_to_string(file_hash_string.as_mut_ptr(), &mut strlength as *mut _)?;

        // Ensure the string is uppercase for WinVerifyTrust
        unsafe {
            CharUpperBuffW(
                file_hash_string.as_mut_ptr(),
                file_hash_string.len().try_into().unwrap(),
            )
        };

        let mut info = WINTRUST_CATALOG_INFO {
            cbStruct: std::mem::size_of::<WINTRUST_CATALOG_INFO>()
                .try_into()
                .unwrap(),
            dwCatalogVersion: 0,
            pcwszCatalogFilePath: catalog_info.wszCatalogFile.as_ptr(),
            pcwszMemberTag: file_hash_string.as_ptr(),
            pcwszMemberFilePath: binary_wide.pcwstr(),
            hMemberFile: file.as_raw_handle() as _,
            // These two weren't used in Authenticode.cpp, though we have the information.
            pbCalculatedFileHash: std::ptr::null_mut(),
            cbCalculatedFileHash: 0,
            pcCatalogContext: std::ptr::null_mut(),
            hCatAdmin: *cat_admin,
        };

        verify_trust(|data| {
            data.dwUnionChoice = WTD_CHOICE_CATALOG;
            data.Anonymous = WINTRUST_DATA_0 {
                pCatalog: &mut info,
            };
        })
    };

    if !verified {
        log::warn!("could not verify trust for {}", binary.display());
        return None;
    }

    let cert_context = crypt_msg
        .get_certificate_info()
        .and_then(|info| cert_store.find_certificate(&info))?;

    cert_context
        .get_name_string()
        .and_then(|oss| oss.into_string().ok())
}

/// Call WinVerifyTrust, first calling `setup` on the `WINTRUST_DATA`. This is expected to set the
/// `dwUnionChoice` and `u` members.
fn verify_trust<F>(setup: F) -> bool
where
    F: FnOnce(&mut WINTRUST_DATA),
{
    let mut guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    let mut data = WINTRUST_DATA {
        cbStruct: std::mem::size_of::<WINTRUST_DATA>().try_into().unwrap(),
        pPolicyCallbackData: std::ptr::null_mut(),
        pSIPClientData: std::ptr::null_mut(),
        dwUIChoice: WTD_UI_NONE,
        fdwRevocationChecks: WTD_REVOKE_NONE,
        // setup should set these two fields
        dwUnionChoice: Default::default(),
        Anonymous: unsafe { std::mem::zeroed::<WINTRUST_DATA_0>() },
        dwStateAction: WTD_STATEACTION_VERIFY,
        hWVTStateData: 0,
        pwszURLReference: std::ptr::null_mut(),
        dwProvFlags: WTD_CACHE_ONLY_URL_RETRIEVAL,
        dwUIContext: 0,
        pSignatureSettings: std::ptr::null_mut(),
    };
    setup(&mut data);

    let result = unsafe {
        WinVerifyTrust(
            INVALID_HANDLE_VALUE as HWND,
            &mut guid,
            &mut data as *mut WINTRUST_DATA as *mut _,
        )
    };

    data.dwStateAction = WTD_STATEACTION_CLOSE;
    unsafe {
        WinVerifyTrust(
            INVALID_HANDLE_VALUE as HWND,
            &mut guid,
            &mut data as *mut WINTRUST_DATA as *mut _,
        )
    };

    result == 0
}

/// Convenience trait for handling windows results.
trait HResult {
    type Value;

    fn into_option(self) -> Option<Self::Value>;
}

impl HResult for BOOL {
    type Value = ();

    fn into_option(self) -> Option<Self::Value> {
        (self != 0).then_some(())
    }
}

/// A certificate store handle.
struct CertStore(HCERTSTORE);

impl Default for CertStore {
    fn default() -> Self {
        CertStore(std::ptr::null_mut())
    }
}

impl CertStore {
    pub fn find_certificate(&self, cert_info: &CertInfo) -> Option<CertContext> {
        let ctx = unsafe {
            CertFindCertificateInStore(
                self.0,
                X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                0,
                CERT_FIND_SUBJECT_CERT,
                cert_info.as_ptr() as *const _,
                std::ptr::null_mut(),
            )
        };
        if ctx.is_null() {
            None
        } else {
            Some(CertContext(ctx))
        }
    }
}

impl std::ops::Deref for CertStore {
    type Target = HCERTSTORE;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl std::ops::DerefMut for CertStore {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl Drop for CertStore {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { CertCloseStore(self.0, 0) };
        }
    }
}

/// A crypt message handle.
type HCRYPTMSG = *mut c_void;
struct CryptMsg(HCRYPTMSG);

impl Default for CryptMsg {
    fn default() -> Self {
        CryptMsg(std::ptr::null_mut())
    }
}

impl CryptMsg {
    pub fn get_certificate_info(&self) -> Option<CertInfo> {
        let mut cert_info_length: DWORD = 0;
        unsafe {
            CryptMsgGetParam(
                self.0,
                CMSG_SIGNER_CERT_INFO_PARAM,
                0,
                std::ptr::null_mut(),
                &mut cert_info_length,
            )
        }
        .into_option()?;

        let mut buffer = vec![0u8; cert_info_length as usize];

        unsafe {
            CryptMsgGetParam(
                self.0,
                CMSG_SIGNER_CERT_INFO_PARAM,
                0,
                buffer.as_mut_ptr() as *mut _,
                &mut cert_info_length,
            )
        }
        .into_option()?;
        buffer.resize(cert_info_length as usize, 0);

        Some(CertInfo { buffer })
    }
}

impl std::ops::Deref for CryptMsg {
    type Target = HCRYPTMSG;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl std::ops::DerefMut for CryptMsg {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl Drop for CryptMsg {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { CryptMsgClose(self.0) };
        }
    }
}

/// Certificate information.
struct CertInfo {
    buffer: Vec<u8>,
}

impl CertInfo {
    pub fn as_ptr(&self) -> *const u8 {
        self.buffer.as_ptr()
    }
}

/// A certificate context.
#[allow(non_camel_case_types)]
type PCCERT_CONTEXT = *const CERT_CONTEXT;
struct CertContext(PCCERT_CONTEXT);

impl CertContext {
    pub fn get_name_string(&self) -> Option<OsString> {
        let char_count = unsafe {
            CertGetNameStringW(
                self.0,
                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                0,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                0,
            )
        };
        if char_count <= 1 {
            return None;
        }
        let mut name = vec![0u16; char_count as usize];
        let char_count = unsafe {
            CertGetNameStringW(
                self.0,
                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                0,
                std::ptr::null_mut(),
                name.as_mut_ptr(),
                char_count,
            )
        };
        assert!(char_count > 1);

        // Subtract one for the null termination byte.
        Some(OsString::from_wide(&name[0..(char_count as usize - 1)]))
    }
}

impl std::ops::Deref for CertContext {
    type Target = PCCERT_CONTEXT;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl std::ops::DerefMut for CertContext {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl Drop for CertContext {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { CertFreeCertificateContext(self.0) };
        }
    }
}
