// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{mem, os::raw::c_void, pin::Pin};

use enum_map::EnumMap;
use neqo_common::qdebug;
use strum::FromRepr;

use crate::{
    agentio::as_c_void,
    constants::Epoch,
    err::Res,
    p11::{PK11SymKey, PK11_ReferenceSymKey, SymKey},
    ssl::{PRFileDesc, SSLSecretCallback, SSLSecretDirection},
};

experimental_api!(SSL_SecretCallback(
    fd: *mut PRFileDesc,
    cb: SSLSecretCallback,
    arg: *mut c_void,
));

#[derive(Clone, Copy, Debug, FromRepr)]
#[cfg_attr(windows, repr(i32))] // Windows has to be different, of course.
#[cfg_attr(not(windows), repr(u32))]
pub enum SecretDirection {
    Read = SSLSecretDirection::ssl_secret_read,
    Write = SSLSecretDirection::ssl_secret_write,
}

impl From<SSLSecretDirection::Type> for SecretDirection {
    fn from(dir: SSLSecretDirection::Type) -> Self {
        Self::from_repr(dir).expect("Invalid secret direction")
    }
}

#[derive(Debug, Default)]
pub struct DirectionalSecrets {
    secrets: EnumMap<Epoch, SymKey>,
}

impl DirectionalSecrets {
    fn put(&mut self, epoch: Epoch, key: SymKey) {
        debug_assert!(epoch != Epoch::Initial);
        self.secrets[epoch] = key;
    }

    pub fn take(&mut self, epoch: Epoch) -> Option<SymKey> {
        if self.secrets[epoch].is_null() {
            None
        } else {
            Some(mem::take(&mut self.secrets[epoch]))
        }
    }
}

#[derive(Debug, Default)]
pub struct Secrets {
    r: DirectionalSecrets,
    w: DirectionalSecrets,
}

impl Secrets {
    unsafe extern "C" fn secret_available(
        _fd: *mut PRFileDesc,
        epoch: u16,
        dir: SSLSecretDirection::Type,
        secret: *mut PK11SymKey,
        arg: *mut c_void,
    ) {
        let Ok(epoch) = Epoch::try_from(epoch) else {
            debug_assert!(false, "Invalid epoch");
            // Don't touch secrets.
            return;
        };
        let Some(secrets) = arg.cast::<Self>().as_mut() else {
            debug_assert!(false, "No secrets");
            return;
        };
        secrets.put_raw(epoch, dir, secret);
    }

    fn put_raw(&mut self, epoch: Epoch, dir: SSLSecretDirection::Type, key_ptr: *mut PK11SymKey) {
        let key_ptr = unsafe { PK11_ReferenceSymKey(key_ptr) };
        let key = SymKey::from_ptr(key_ptr).expect("NSS shouldn't be passing out NULL secrets");
        self.put(SecretDirection::from(dir), epoch, key);
    }

    fn put(&mut self, dir: SecretDirection, epoch: Epoch, key: SymKey) {
        qdebug!("{dir:?} secret available for {epoch:?}: {key:?}");
        let keys = match dir {
            SecretDirection::Read => &mut self.r,
            SecretDirection::Write => &mut self.w,
        };
        keys.put(epoch, key);
    }
}

#[derive(Debug)]
pub struct SecretHolder {
    secrets: Pin<Box<Secrets>>,
}

impl SecretHolder {
    /// This registers with NSS.  The lifetime of this object needs to match the lifetime
    /// of the connection, or bad things might happen.
    pub fn register(&mut self, fd: *mut PRFileDesc) -> Res<()> {
        let p = as_c_void(&mut self.secrets);
        unsafe { SSL_SecretCallback(fd, Some(Secrets::secret_available), p) }
    }

    pub fn take_read(&mut self, epoch: Epoch) -> Option<SymKey> {
        self.secrets.r.take(epoch)
    }

    pub fn take_write(&mut self, epoch: Epoch) -> Option<SymKey> {
        self.secrets.w.take(epoch)
    }
}

impl Default for SecretHolder {
    fn default() -> Self {
        Self {
            secrets: Box::pin(Secrets::default()),
        }
    }
}
