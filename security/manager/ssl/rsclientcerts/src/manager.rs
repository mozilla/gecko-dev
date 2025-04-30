/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use pkcs11_bindings::*;
use std::collections::{BTreeMap, BTreeSet};
use std::time::{Duration, Instant};

use crate::error::{Error, ErrorType};
use crate::error_here;

extern "C" {
    fn IsGeckoSearchingForClientAuthCertificates() -> bool;
}

pub trait CryptokiObject {
    fn matches(&self, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool;
    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]>;
}

pub trait Sign {
    fn get_signature_length(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<usize, Error>;
    fn sign(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<Vec<u8>, Error>;
}

pub trait ClientCertsBackend {
    type Cert: CryptokiObject;
    type Key: CryptokiObject + Sign;

    #[allow(clippy::type_complexity)]
    fn find_objects(&self) -> Result<(Vec<Self::Cert>, Vec<Self::Key>), Error>;
}

const SUPPORTED_ATTRIBUTES: &[CK_ATTRIBUTE_TYPE] = &[
    CKA_CLASS,
    CKA_TOKEN,
    CKA_LABEL,
    CKA_ID,
    CKA_VALUE,
    CKA_ISSUER,
    CKA_SERIAL_NUMBER,
    CKA_SUBJECT,
    CKA_PRIVATE,
    CKA_KEY_TYPE,
    CKA_MODULUS,
    CKA_EC_PARAMS,
];

enum Object<B: ClientCertsBackend> {
    Cert(B::Cert),
    Key(B::Key),
}

impl<B: ClientCertsBackend> Object<B> {
    fn matches(&self, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        match self {
            Object::Cert(cert) => cert.matches(attrs),
            Object::Key(key) => key.matches(attrs),
        }
    }

    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        match self {
            Object::Cert(cert) => cert.get_attribute(attribute),
            Object::Key(key) => key.get_attribute(attribute),
        }
    }

    fn id(&self) -> Result<&[u8], Error> {
        self.get_attribute(CKA_ID)
            .ok_or_else(|| error_here!(ErrorType::LibraryFailure))
    }

    fn get_signature_length(
        &mut self,
        data: Vec<u8>,
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<usize, Error> {
        match self {
            Object::Cert(_) => Err(error_here!(ErrorType::InvalidArgument)),
            Object::Key(key) => key.get_signature_length(&data, params),
        }
    }

    fn sign(
        &mut self,
        data: Vec<u8>,
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<Vec<u8>, Error> {
        match self {
            Object::Cert(_) => Err(error_here!(ErrorType::InvalidArgument)),
            Object::Key(key) => key.sign(&data, params),
        }
    }
}

/// The `Manager` keeps track of the state of this module with respect to the PKCS #11
/// specification. This includes what sessions are open, which search and sign operations are
/// ongoing, and what objects are known and by what handle.
pub struct Manager<B: ClientCertsBackend> {
    /// A set of open sessions. Sessions can be created (opened) and later closed.
    sessions: BTreeSet<CK_SESSION_HANDLE>,
    /// A map of searches to PKCS #11 object handles that match those searches.
    searches: BTreeMap<CK_SESSION_HANDLE, Vec<CK_OBJECT_HANDLE>>,
    /// A map of sign operations to a pair of the object handle and optionally some params being
    /// used by each one.
    signs: BTreeMap<CK_SESSION_HANDLE, (CK_OBJECT_HANDLE, Option<CK_RSA_PKCS_PSS_PARAMS>)>,
    /// A map of object handles to the underlying objects.
    objects: BTreeMap<CK_OBJECT_HANDLE, Object<B>>,
    /// A set of certificate identifiers (not the same as handles).
    cert_ids: BTreeSet<Vec<u8>>,
    /// A set of key identifiers (not the same as handles). For each id in this set, there should be
    /// a corresponding identical id in the `cert_ids` set.
    key_ids: BTreeSet<Vec<u8>>,
    /// The next session handle to hand out.
    next_session: CK_SESSION_HANDLE,
    /// The next object handle to hand out.
    next_handle: CK_OBJECT_HANDLE,
    /// The last time the implementation finished looking for new objects in the backend.
    /// The implementation does this search no more than once every 3 seconds.
    last_scan_finished: Option<Instant>,
    backend: B,
}

impl<B: ClientCertsBackend> Manager<B> {
    pub fn new(backend: B) -> Manager<B> {
        Manager {
            sessions: BTreeSet::new(),
            searches: BTreeMap::new(),
            signs: BTreeMap::new(),
            objects: BTreeMap::new(),
            cert_ids: BTreeSet::new(),
            key_ids: BTreeSet::new(),
            next_session: 1,
            next_handle: 1,
            last_scan_finished: None,
            backend,
        }
    }

    /// When a new search session is opened (provided at least 3 seconds have elapsed since the
    /// last search finished), this searches for certificates and keys to expose. We de-duplicate
    /// previously-found certificates and keys by keeping track of their IDs.
    fn maybe_find_new_objects(&mut self) -> Result<(), Error> {
        match self.last_scan_finished {
            Some(last_scan_finished) => {
                if Instant::now().duration_since(last_scan_finished) < Duration::new(3, 0) {
                    return Ok(());
                }
            }
            None => {}
        }
        let (certs, keys) = self.backend.find_objects()?;
        self.last_scan_finished = Some(Instant::now());
        for cert in certs {
            let object = Object::Cert(cert);
            if self.cert_ids.contains(object.id()?) {
                continue;
            }
            self.cert_ids.insert(object.id()?.to_vec());
            let handle = self.get_next_handle();
            self.objects.insert(handle, object);
        }
        for key in keys {
            let object = Object::Key(key);
            if self.key_ids.contains(object.id()?) {
                continue;
            }
            self.key_ids.insert(object.id()?.to_vec());
            let handle = self.get_next_handle();
            self.objects.insert(handle, object);
        }
        Ok(())
    }

    pub fn open_session(&mut self) -> Result<CK_SESSION_HANDLE, Error> {
        let next_session = self.next_session;
        self.next_session += 1;
        self.sessions.insert(next_session);
        Ok(next_session)
    }

    pub fn close_session(&mut self, session: CK_SESSION_HANDLE) -> Result<(), Error> {
        if !self.sessions.remove(&session) {
            return Err(error_here!(ErrorType::InvalidInput));
        }
        Ok(())
    }

    pub fn close_all_sessions(&mut self) -> Result<(), Error> {
        self.sessions.clear();
        Ok(())
    }

    fn get_next_handle(&mut self) -> CK_OBJECT_HANDLE {
        let next_handle = self.next_handle;
        self.next_handle += 1;
        next_handle
    }

    /// PKCS #11 specifies that search operations happen in three phases: setup, get any matches
    /// (this part may be repeated if the caller uses a small buffer), and end. This implementation
    /// does all of the work up front and gathers all matching objects during setup and retains them
    /// until they are retrieved and consumed via `search`.
    pub fn start_search(
        &mut self,
        session: CK_SESSION_HANDLE,
        attrs: Vec<(CK_ATTRIBUTE_TYPE, Vec<u8>)>,
    ) -> Result<(), Error> {
        if !self.sessions.contains(&session) {
            return Err(error_here!(ErrorType::InvalidArgument));
        }
        // If the search is for an attribute we don't support, no objects will match. This check
        // saves us having to look through all of our objects.
        for (attr, _) in &attrs {
            if !SUPPORTED_ATTRIBUTES.contains(attr) {
                self.searches.insert(session, Vec::new());
                return Ok(());
            }
        }
        // Only search for new objects when gecko has indicated that it is looking for client
        // authentication certificates (or all certificates).
        // Since these searches are relatively rare, this minimizes the impact of doing these
        // re-scans.
        if unsafe { IsGeckoSearchingForClientAuthCertificates() } {
            self.maybe_find_new_objects()?;
        }
        let mut handles = Vec::new();
        for (handle, object) in &self.objects {
            if object.matches(&attrs) {
                handles.push(*handle);
            }
        }
        self.searches.insert(session, handles);
        Ok(())
    }

    /// Given a session and a maximum number of object handles to return, attempts to retrieve up to
    /// that many objects from the corresponding search. Updates the search so those objects are not
    /// returned repeatedly. `max_objects` must be non-zero.
    pub fn search(
        &mut self,
        session: CK_SESSION_HANDLE,
        max_objects: usize,
    ) -> Result<Vec<CK_OBJECT_HANDLE>, Error> {
        if max_objects == 0 {
            return Err(error_here!(ErrorType::InvalidArgument));
        }
        match self.searches.get_mut(&session) {
            Some(search) => {
                let split_at = if max_objects >= search.len() {
                    0
                } else {
                    search.len() - max_objects
                };
                let to_return = search.split_off(split_at);
                if to_return.len() > max_objects {
                    return Err(error_here!(ErrorType::LibraryFailure));
                }
                Ok(to_return)
            }
            None => Err(error_here!(ErrorType::InvalidArgument)),
        }
    }

    pub fn clear_search(&mut self, session: CK_SESSION_HANDLE) -> Result<(), Error> {
        self.searches.remove(&session);
        Ok(())
    }

    pub fn get_attributes(
        &self,
        object_handle: CK_OBJECT_HANDLE,
        attr_types: Vec<CK_ATTRIBUTE_TYPE>,
    ) -> Result<Vec<Option<Vec<u8>>>, Error> {
        let object = match self.objects.get(&object_handle) {
            Some(object) => object,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        let mut results = Vec::with_capacity(attr_types.len());
        for attr_type in attr_types {
            let result = object
                .get_attribute(attr_type)
                .map(|value| value.to_owned());
            results.push(result);
        }
        Ok(results)
    }

    /// The way NSS uses PKCS #11 to sign data happens in two phases: setup and sign. This
    /// implementation makes a note of which key is to be used (if it exists) during setup. When the
    /// caller finishes with the sign operation, this implementation retrieves the key handle and
    /// performs the signature.
    pub fn start_sign(
        &mut self,
        session: CK_SESSION_HANDLE,
        key_handle: CK_OBJECT_HANDLE,
        params: Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<(), Error> {
        if self.signs.contains_key(&session) {
            return Err(error_here!(ErrorType::InvalidArgument));
        }
        self.signs.insert(session, (key_handle, params));
        Ok(())
    }

    pub fn get_signature_length(
        &mut self,
        session: CK_SESSION_HANDLE,
        data: Vec<u8>,
    ) -> Result<usize, Error> {
        let (key_handle, params) = match self.signs.get(&session) {
            Some((key_handle, params)) => (key_handle, params),
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        let key = match self.objects.get_mut(key_handle) {
            Some(key) => key,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        key.get_signature_length(data, params)
    }

    pub fn sign(&mut self, session: CK_SESSION_HANDLE, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        // Performing the signature (via C_Sign, which is the only way we support) finishes the sign
        // operation, so it needs to be removed here.
        let (key_handle, params) = match self.signs.remove(&session) {
            Some((key_handle, params)) => (key_handle, params),
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        let key = match self.objects.get_mut(&key_handle) {
            Some(key) => key,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        key.sign(data, &params)
    }
}
