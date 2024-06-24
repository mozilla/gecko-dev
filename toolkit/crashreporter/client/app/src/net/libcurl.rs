/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Partial libcurl bindings with some wrappers for safe cleanup.

use crate::std::path::Path;
use libloading::{Library, Symbol};
use once_cell::sync::Lazy;
use std::ffi::{c_char, c_long, c_uint, CStr, CString};

// Constants lifted from `curl.h`
const CURLE_OK: CurlCode = 0;
const CURLE_OUT_OF_MEMORY: CurlCode = 27;
const CURL_ERROR_SIZE: usize = 256;

const CURLOPTTYPE_LONG: CurlOption = 0;
const CURLOPTTYPE_OBJECTPOINT: CurlOption = 10000;
const CURLOPTTYPE_FUNCTIONPOINT: CurlOption = 20000;
const CURLOPTTYPE_STRINGPOINT: CurlOption = CURLOPTTYPE_OBJECTPOINT;
const CURLOPTTYPE_CBPOINT: CurlOption = CURLOPTTYPE_OBJECTPOINT;
const CURLOPTTYPE_SLISTPOINT: CurlOption = CURLOPTTYPE_OBJECTPOINT;

const CURLOPT_WRITEDATA: CurlOption = CURLOPTTYPE_CBPOINT + 1;
const CURLOPT_URL: CurlOption = CURLOPTTYPE_STRINGPOINT + 2;
const CURLOPT_ERRORBUFFER: CurlOption = CURLOPTTYPE_OBJECTPOINT + 10;
const CURLOPT_WRITEFUNCTION: CurlOption = CURLOPTTYPE_FUNCTIONPOINT + 11;
const CURLOPT_USERAGENT: CurlOption = CURLOPTTYPE_STRINGPOINT + 18;
const CURLOPT_MIMEPOST: CurlOption = CURLOPTTYPE_OBJECTPOINT + 269;
const CURLOPT_MAXREDIRS: CurlOption = CURLOPTTYPE_LONG + 68;
const CURLOPT_HTTPHEADER: CurlOption = CURLOPTTYPE_SLISTPOINT + 23;
const CURLOPT_POSTFIELDS: CurlOption = CURLOPTTYPE_OBJECTPOINT + 15;
const CURLOPT_POSTFIELDSIZE: CurlOption = CURLOPTTYPE_LONG + 60;

const CURLINFO_LONG: CurlInfo = 0x200000;
const CURLINFO_RESPONSE_CODE: CurlInfo = CURLINFO_LONG + 2;

const CURL_LIB_NAMES: &[&str] = if cfg!(target_os = "linux") {
    &[
        "libcurl.so",
        "libcurl.so.4",
        // Debian gives libcurl a different name when it is built against GnuTLS
        "libcurl-gnutls.so",
        "libcurl-gnutls.so.4",
        // Older versions in case we find nothing better
        "libcurl.so.3",
        "libcurl-gnutls.so.3", // See above for Debian
    ]
} else if cfg!(target_os = "macos") {
    &[
        "/usr/lib/libcurl.dylib",
        "/usr/lib/libcurl.4.dylib",
        "/usr/lib/libcurl.3.dylib",
    ]
} else if cfg!(target_os = "windows") {
    &["libcurl.dll", "curl.dll"]
} else {
    &[]
};

#[repr(transparent)]
#[derive(Clone, Copy)]
struct CurlHandle(*mut ());
type CurlCode = c_uint;
type CurlOption = c_uint;
type CurlInfo = c_uint;
#[repr(transparent)]
#[derive(Clone, Copy)]
struct CurlMime(*mut ());
#[repr(transparent)]
#[derive(Clone, Copy)]
struct CurlMimePart(*mut ());
#[repr(transparent)]
#[derive(Clone, Copy)]
struct CurlSlist(*mut ());

// # Safety
// Curl handles are safe to pass among threads: https://curl.se/libcurl/c/threadsafe.html.
unsafe impl Send for CurlHandle {}
unsafe impl Send for CurlMime {}
unsafe impl Send for CurlMimePart {}
unsafe impl Send for CurlSlist {}

macro_rules! library_binding {
    ( $localname:ident members[$($members:tt)*] load[$($load:tt)*] fn $name:ident $args:tt $( -> $ret:ty )? ; $($rest:tt)* ) => {
        library_binding! {
            $localname
            members[
                $($members)*
                $name: Symbol<'static, unsafe extern fn $args $(->$ret)?>,
            ]
            load[
                $($load)*
                $name: unsafe {
                    let symbol = $localname.get::<unsafe extern fn $args $(->$ret)?>(stringify!($name).as_bytes())
                    .map_err(|e| std::io::Error::new(std::io::ErrorKind::NotFound, e))?;
                    // All symbols refer to library, so `'static` lifetimes are safe (`library`
                    // will outlive them).
                    std::mem::transmute(symbol)
                },
            ]
            $($rest)*
        }
    };
    ( $localname:ident members[$($members:tt)*] load[$($load:tt)*] ) => {
        pub struct Curl {
            $($members)*
            _library: Library
        }

        impl Curl {
            fn load() -> std::io::Result<Self> {
                // Try each of the libraries, debug-logging load failures.
                let library = CURL_LIB_NAMES.iter().find_map(|name| {
                    log::debug!("attempting to load {name}");
                    match unsafe { Library::new(name) } {
                        Ok(lib) => {
                            log::info!("loaded {name}");
                            Some(lib)
                        }
                        Err(e) => {
                            log::debug!("error when loading {name}: {e}");
                            None
                        }
                    }
                });

                let $localname = library.ok_or_else(|| {
                    std::io::Error::new(std::io::ErrorKind::NotFound, "failed to find curl library")
                })?;

                Ok(Curl { $($load)* _library: $localname })
            }
        }
    };
    ( $($rest:tt)* ) => {
        library_binding! {
            library members[] load[] $($rest)*
        }
    }
}

library_binding! {
    fn curl_easy_init() -> CurlHandle;
    fn curl_easy_setopt(CurlHandle, CurlOption, ...) -> CurlCode;
    fn curl_easy_perform(CurlHandle) -> CurlCode;
    fn curl_easy_getinfo(CurlHandle, CurlInfo, ...) -> CurlCode;
    fn curl_easy_cleanup(CurlHandle);
    fn curl_mime_init(CurlHandle) -> CurlMime;
    fn curl_mime_addpart(CurlMime) -> CurlMimePart;
    fn curl_mime_name(CurlMimePart, *const c_char) -> CurlCode;
    fn curl_mime_filename(CurlMimePart, *const c_char) -> CurlCode;
    fn curl_mime_type(CurlMimePart, *const c_char) -> CurlCode;
    fn curl_mime_data(CurlMimePart, *const c_char, usize) -> CurlCode;
    fn curl_mime_filedata(CurlMimePart, *const c_char) -> CurlCode;
    fn curl_mime_free(CurlMime);
    fn curl_slist_append(CurlSlist, *const c_char) -> CurlSlist;
    fn curl_slist_free_all(CurlSlist);
}

/// Load libcurl if possible.
pub fn load() -> std::io::Result<&'static Curl> {
    static CURL: Lazy<std::io::Result<Curl>> = Lazy::new(Curl::load);
    CURL.as_ref().map_err(std::io::Error::other)
}

#[derive(Debug)]
pub struct Error {
    code: CurlCode,
    error: Option<String>,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "curl error code {}", self.code)?;
        if let Some(e) = &self.error {
            write!(f, ": {e}")?;
        }
        Ok(())
    }
}

impl std::error::Error for Error {}

impl From<Error> for std::io::Error {
    fn from(e: Error) -> Self {
        std::io::Error::other(e)
    }
}

pub type Result<T> = std::result::Result<T, Error>;

fn to_result(code: CurlCode) -> Result<()> {
    if code == CURLE_OK {
        Ok(())
    } else {
        Err(Error { code, error: None })
    }
}

impl Curl {
    pub fn easy(&self) -> std::io::Result<Easy> {
        let handle = unsafe { (self.curl_easy_init)() };
        if handle.0.is_null() {
            Err(std::io::Error::other("curl_easy_init failed"))
        } else {
            Ok(Easy {
                lib: self,
                handle,
                mime: None,
                headers: None,
                postdata: None,
            })
        }
    }
}

struct ErrorBuffer([u8; CURL_ERROR_SIZE]);

impl Default for ErrorBuffer {
    fn default() -> Self {
        ErrorBuffer([0; CURL_ERROR_SIZE])
    }
}

pub struct Easy<'a> {
    lib: &'a Curl,
    handle: CurlHandle,
    mime: Option<Mime<'a>>,
    headers: Option<Slist<'a>>,
    postdata: Option<Box<[u8]>>,
}

impl<'a> Easy<'a> {
    pub fn set_url(&mut self, url: &str) -> Result<()> {
        let url = CString::new(url).unwrap();
        to_result(unsafe { (self.lib.curl_easy_setopt)(self.handle, CURLOPT_URL, url.as_ptr()) })
    }

    pub fn set_user_agent(&mut self, user_agent: &str) -> Result<()> {
        let ua = CString::new(user_agent).unwrap();
        to_result(unsafe {
            (self.lib.curl_easy_setopt)(self.handle, CURLOPT_USERAGENT, ua.as_ptr())
        })
    }

    pub fn mime(&self) -> std::io::Result<Mime<'a>> {
        let handle = unsafe { (self.lib.curl_mime_init)(self.handle) };
        if handle.0.is_null() {
            Err(std::io::Error::other("curl_mime_init failed"))
        } else {
            Ok(Mime {
                lib: self.lib,
                handle,
            })
        }
    }

    pub fn set_mime_post(&mut self, mime: Mime<'a>) -> Result<()> {
        to_result(unsafe {
            (self.lib.curl_easy_setopt)(self.handle, CURLOPT_MIMEPOST, mime.handle)
        })?;
        self.mime = Some(mime);
        Ok(())
    }

    pub fn set_max_redirs(&mut self, redirs: c_long) -> Result<()> {
        to_result(unsafe { (self.lib.curl_easy_setopt)(self.handle, CURLOPT_MAXREDIRS, redirs) })
    }

    /// Create a new, empty string list.
    pub fn slist(&self) -> Slist<'a> {
        Slist {
            lib: self.lib,
            handle: CurlSlist(std::ptr::null_mut()),
        }
    }

    pub fn set_headers(&mut self, headers: Slist<'a>) -> Result<()> {
        to_result(unsafe {
            (self.lib.curl_easy_setopt)(self.handle, CURLOPT_HTTPHEADER, headers.handle)
        })?;
        self.headers = Some(headers);
        Ok(())
    }

    pub fn set_postfields(&mut self, data: impl Into<Box<[u8]>>) -> std::io::Result<()> {
        let data = data.into();
        let size: c_long = data.len().try_into().map_err(std::io::Error::other)?;
        to_result(unsafe {
            (self.lib.curl_easy_setopt)(self.handle, CURLOPT_POSTFIELDSIZE, size)
        })?;
        to_result(unsafe {
            (self.lib.curl_easy_setopt)(
                self.handle,
                CURLOPT_POSTFIELDS,
                data.as_ptr() as *const c_char,
            )
        })?;
        self.postdata = Some(data);
        Ok(())
    }

    /// Returns the response data on success.
    pub fn perform(&self) -> Result<Vec<u8>> {
        // Set error buffer, but degrade service if it doesn't work.
        let mut error_buffer = ErrorBuffer::default();
        let error_buffer_set = unsafe {
            (self.lib.curl_easy_setopt)(
                self.handle,
                CURLOPT_ERRORBUFFER,
                error_buffer.0.as_mut_ptr() as *mut c_char,
            )
        } == CURLE_OK;

        // Set the write function to fill a Vec. If there is a panic, this might leave stale
        // pointers in the curl options, but they won't be used without another perform, at which
        // point they'll be overwritten.
        let mut data: Vec<u8> = Vec::new();
        extern "C" fn write_callback(
            data: *const u8,
            size: usize,
            nmemb: usize,
            dest: &mut Vec<u8>,
        ) -> usize {
            let total = size * nmemb;
            dest.extend(unsafe { std::slice::from_raw_parts(data, total) });
            total
        }
        unsafe {
            to_result((self.lib.curl_easy_setopt)(
                self.handle,
                CURLOPT_WRITEFUNCTION,
                write_callback as extern "C" fn(*const u8, usize, usize, &mut Vec<u8>) -> usize,
            ))?;
            to_result((self.lib.curl_easy_setopt)(
                self.handle,
                CURLOPT_WRITEDATA,
                &mut data as *mut _,
            ))?;
        };

        let mut result = to_result(unsafe { (self.lib.curl_easy_perform)(self.handle) });

        // Clean up a bit by unsetting the write function and write data, though they won't be used
        // anywhere else. Ignore return values.
        unsafe {
            (self.lib.curl_easy_setopt)(
                self.handle,
                CURLOPT_WRITEFUNCTION,
                std::ptr::null_mut::<()>(),
            );
            (self.lib.curl_easy_setopt)(self.handle, CURLOPT_WRITEDATA, std::ptr::null_mut::<()>());
        }

        if error_buffer_set {
            unsafe {
                (self.lib.curl_easy_setopt)(
                    self.handle,
                    CURLOPT_ERRORBUFFER,
                    std::ptr::null_mut::<()>(),
                )
            };
            if let Err(e) = &mut result {
                if let Ok(cstr) = CStr::from_bytes_until_nul(error_buffer.0.as_slice()) {
                    e.error = Some(cstr.to_string_lossy().into_owned());
                }
            }
        }

        result.map(move |()| data)
    }

    pub fn get_response_code(&self) -> Result<u64> {
        let mut code = c_long::default();
        to_result(unsafe {
            (self.lib.curl_easy_getinfo)(
                self.handle,
                CURLINFO_RESPONSE_CODE,
                &mut code as *mut c_long,
            )
        })?;
        Ok(code.try_into().expect("negative http response code"))
    }
}

impl Drop for Easy<'_> {
    fn drop(&mut self) {
        self.mime.take();
        unsafe { (self.lib.curl_easy_cleanup)(self.handle) };
    }
}

pub struct Mime<'a> {
    lib: &'a Curl,
    handle: CurlMime,
}

impl<'a> Mime<'a> {
    pub fn add_part(&mut self) -> std::io::Result<MimePart<'a>> {
        let handle = unsafe { (self.lib.curl_mime_addpart)(self.handle) };
        if handle.0.is_null() {
            Err(std::io::Error::other("curl_mime_addpart failed"))
        } else {
            Ok(MimePart {
                lib: self.lib,
                handle,
            })
        }
    }
}

impl Drop for Mime<'_> {
    fn drop(&mut self) {
        unsafe { (self.lib.curl_mime_free)(self.handle) };
    }
}

pub struct MimePart<'a> {
    lib: &'a Curl,
    handle: CurlMimePart,
}

impl MimePart<'_> {
    pub fn set_name(&mut self, name: &str) -> Result<()> {
        let name = CString::new(name).unwrap();
        to_result(unsafe { (self.lib.curl_mime_name)(self.handle, name.as_ptr()) })
    }

    pub fn set_filename(&mut self, filename: &str) -> Result<()> {
        let filename = CString::new(filename).unwrap();
        to_result(unsafe { (self.lib.curl_mime_filename)(self.handle, filename.as_ptr()) })
    }

    pub fn set_type(&mut self, mime_type: &str) -> Result<()> {
        let mime_type = CString::new(mime_type).unwrap();
        to_result(unsafe { (self.lib.curl_mime_type)(self.handle, mime_type.as_ptr()) })
    }

    pub fn set_filedata(&mut self, file: &Path) -> Result<()> {
        let file = CString::new(file.display().to_string()).unwrap();
        to_result(unsafe { (self.lib.curl_mime_filedata)(self.handle, file.as_ptr()) })
    }

    pub fn set_data(&mut self, data: &[u8]) -> Result<()> {
        to_result(unsafe {
            (self.lib.curl_mime_data)(self.handle, data.as_ptr() as *const c_char, data.len())
        })
    }
}

pub struct Slist<'a> {
    lib: &'a Curl,
    handle: CurlSlist,
}

impl Slist<'_> {
    pub fn append(&mut self, s: &str) -> Result<()> {
        let cs = CString::new(s).unwrap();
        let new_handle = unsafe { (self.lib.curl_slist_append)(self.handle, cs.as_ptr()) };
        if new_handle.0.is_null() {
            return Err(Error {
                // From source inspection, failure modes are all malloc errors,
                // which are more than likely only from OOM.
                code: CURLE_OUT_OF_MEMORY,
                error: Some(format!("failed to append {s} to slist")),
            });
        }
        self.handle = new_handle;
        Ok(())
    }
}

impl Drop for Slist<'_> {
    fn drop(&mut self) {
        unsafe { (self.lib.curl_slist_free_all)(self.handle) };
    }
}
