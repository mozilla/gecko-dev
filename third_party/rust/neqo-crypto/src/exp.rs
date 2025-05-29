// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[macro_export]
macro_rules! experimental_api {
    ( $n:ident ( $( $a:ident : $t:ty ),* $(,)? ) ) => {
        #[expect(non_snake_case, reason = "Inherent in macro use.")]
        #[allow(clippy::allow_attributes, clippy::too_many_arguments, reason = "Inherent in macro use.")]
        #[allow(clippy::allow_attributes, clippy::missing_safety_doc, reason = "Inherent in macro use.")]
        #[allow(clippy::allow_attributes, clippy::missing_errors_doc, reason = "Inherent in macro use.")]
        pub unsafe fn $n ( $( $a : $t ),* ) -> Result<(), $crate::err::Error> {
            struct ExperimentalAPI(*mut std::ffi::c_void);
            unsafe impl Send for ExperimentalAPI {}
            unsafe impl Sync for ExperimentalAPI {}
            static EXP_API: ::std::sync::OnceLock<ExperimentalAPI> = ::std::sync::OnceLock::new();
            let f = EXP_API.get_or_init(|| {
                const EXP_FUNCTION: &str = stringify!($n);
                let Ok(n) = ::std::ffi::CString::new(EXP_FUNCTION) else { return ExperimentalAPI(std::ptr::null_mut()); };
                ExperimentalAPI($crate::ssl::SSL_GetExperimentalAPI(n.as_ptr()))
            });
            if f.0.is_null() {
                return Err($crate::err::Error::InternalError);
            }
            let f: unsafe extern "C" fn( $( $t ),* ) -> $crate::ssl::SECStatus = ::std::mem::transmute(f.0);
            let rv = f( $( $a ),* );
            $crate::err::secstatus_to_res(rv)
        }
    };
}
