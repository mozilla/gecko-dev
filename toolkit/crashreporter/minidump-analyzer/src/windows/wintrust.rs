/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::strings::utf16_ptr_to_ascii;
use super::HResult;
use std::fs::File;
use std::os::windows::io::AsRawHandle;
use windows_sys::Win32::{
    Foundation::{GetLastError, BOOL, ERROR_INSUFFICIENT_BUFFER, HANDLE, MAX_PATH},
    Security::Cryptography::{
        szOID_CERT_STRONG_SIGN_OS_1, szOID_CERT_STRONG_SIGN_OS_CURRENT,
        Catalog::{
            CryptCATAdminAcquireContext2, CryptCATAdminCalcHashFromFileHandle2,
            CryptCATAdminEnumCatalogFromHash, CryptCATAdminReleaseCatalogContext,
            CryptCATAdminReleaseContext, CryptCATCatalogInfoFromContext, CATALOG_INFO,
        },
        BCRYPT_SHA256_ALGORITHM, CERT_STRONG_SIGN_OID_INFO_CHOICE, CERT_STRONG_SIGN_PARA,
        CERT_STRONG_SIGN_PARA_0,
    },
};

pub type HCATADMIN = HANDLE;
pub type HCATINFO = HANDLE;

/// A catalog admin handle.
pub struct CATAdmin(HCATADMIN);

impl Default for CATAdmin {
    fn default() -> Self {
        CATAdmin(0)
    }
}

impl CATAdmin {
    /// Acquire a handle.
    pub fn acquire() -> Option<Self> {
        let mut ret = Self::default();
        // Annoyingly, szOID_CERT_STRONG_SIGN_OS_CURRENT is a wide string, but all other such
        // constants are C strings.
        let oid_string = utf16_ptr_to_ascii(szOID_CERT_STRONG_SIGN_OS_CURRENT);
        let policy = CERT_STRONG_SIGN_PARA {
            cbSize: std::mem::size_of::<CERT_STRONG_SIGN_PARA>() as u32,
            dwInfoChoice: CERT_STRONG_SIGN_OID_INFO_CHOICE,
            Anonymous: CERT_STRONG_SIGN_PARA_0 {
                pszOID: oid_string
                    .as_ref()
                    .map(|c| c.as_ptr() as *mut u8)
                    .unwrap_or(szOID_CERT_STRONG_SIGN_OS_1 as *mut u8),
            },
        };
        unsafe {
            CryptCATAdminAcquireContext2(
                &mut *ret,
                std::ptr::null(),
                BCRYPT_SHA256_ALGORITHM,
                &policy as *const _,
                0,
            )
        }
        .into_option()?;
        Some(ret)
    }

    /// Calculate the hash of the given file.
    pub fn calculate_file_hash(&self, file: &File) -> Option<Vec<u8>> {
        let calc_hash = |size: *mut u32, dest: *mut u8| -> BOOL {
            unsafe {
                CryptCATAdminCalcHashFromFileHandle2(
                    self.0,
                    file.as_raw_handle() as _,
                    size,
                    dest,
                    0,
                )
            }
        };

        // First call to retrieve the hash size.
        let mut size: u32 = 0;
        calc_hash(&mut size, std::ptr::null_mut())
            .into_option()
            // If ERROR_INSUFFICIENT_BUFFER is the last error, `size` has been set.
            .or_else(|| (unsafe { GetLastError() } == ERROR_INSUFFICIENT_BUFFER).then_some(()))?;

        // Second call to get the hash.
        let mut hash = vec![0; size as usize];
        calc_hash(&mut size as *mut _, hash.as_mut_ptr()).into_option()?;

        Some(hash)
    }

    /// Find the first catalog that contains the given hash.
    pub fn catalog_from_hash(&self, hash: &mut [u8]) -> Option<CATInfo> {
        let ptr = unsafe {
            CryptCATAdminEnumCatalogFromHash(
                self.0,
                hash.as_mut_ptr(),
                hash.len().try_into().unwrap(),
                0,
                std::ptr::null_mut(),
            )
        };
        if ptr == 0 {
            None
        } else {
            Some(CATInfo {
                cat_admin_context: self,
                handle: ptr,
            })
        }
    }
}

impl std::ops::Deref for CATAdmin {
    type Target = HCATADMIN;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl std::ops::DerefMut for CATAdmin {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl Drop for CATAdmin {
    fn drop(&mut self) {
        if self.0 != 0 {
            unsafe { CryptCATAdminReleaseContext(self.0, 0) };
        }
    }
}

pub struct CATInfo<'a> {
    cat_admin_context: &'a CATAdmin,
    handle: HCATINFO,
}

impl CATInfo<'_> {
    pub fn get_info(&self) -> Option<CATALOG_INFO> {
        let mut ret = CATALOG_INFO {
            cbStruct: std::mem::size_of::<CATALOG_INFO>().try_into().unwrap(),
            wszCatalogFile: [0u16; MAX_PATH as usize],
        };
        unsafe { CryptCATCatalogInfoFromContext(self.handle, &mut ret as *mut _, 0) }
            .into_option()?;
        Some(ret)
    }
}

impl std::ops::Deref for CATInfo<'_> {
    type Target = HCATINFO;

    fn deref(&self) -> &Self::Target {
        &self.handle
    }
}

impl std::ops::DerefMut for CATInfo<'_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.handle
    }
}

impl Drop for CATInfo<'_> {
    fn drop(&mut self) {
        // Unwrap because this function must exist if we got a handle.
        unsafe { CryptCATAdminReleaseCatalogContext(**self.cat_admin_context, self.handle, 0) };
    }
}
