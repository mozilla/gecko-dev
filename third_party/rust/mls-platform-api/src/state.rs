// Copyright (c) 2024 Mozilla Corporation and contributors.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use std::{
    collections::HashMap,
    path::Path,
    sync::{Arc, Mutex},
};

use mls_rs::{
    client_builder::MlsConfig,
    crypto::{SignaturePublicKey, SignatureSecretKey},
    error::IntoAnyError,
    identity::{Credential, SigningIdentity},
    mls_rules::{CommitOptions, DefaultMlsRules},
    storage_provider::KeyPackageData,
    CipherSuite, Client, ProtocolVersion,
};

use crate::ClientConfig;

use mls_rs_provider_sqlite::{
    connection_strategy::{
        CipheredConnectionStrategy, ConnectionStrategy, FileConnectionStrategy, SqlCipherConfig,
        SqlCipherKey,
    },
    SqLiteDataStorageEngine,
};

use crate::{Identity, PlatformError};

#[derive(serde::Serialize, serde::Deserialize, Clone)]
pub struct GroupData {
    state_data: Vec<u8>,
    epoch_data: HashMap<u64, Vec<u8>>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone)]
pub struct PlatformState {
    pub db_path: String,
    pub db_key: [u8; 32],
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Default)]
pub struct TemporaryState {
    pub groups: Arc<Mutex<HashMap<Vec<u8>, GroupData>>>,
    /// signing identity => key data
    pub sigkeys: HashMap<Vec<u8>, SignatureData>,
    pub key_packages: Arc<Mutex<HashMap<Vec<u8>, KeyPackageData>>>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone)]
pub struct SignatureData {
    #[serde(with = "hex::serde")]
    pub public_key: Vec<u8>,
    pub cs: u16,
    #[serde(with = "hex::serde")]
    pub secret_key: Vec<u8>,
    pub credential: Option<Credential>,
}

impl PlatformState {
    pub fn new(db_path: &str, db_key: &[u8; 32]) -> Result<Self, PlatformError> {
        let state = Self {
            db_path: db_path.to_string(),
            db_key: *db_key,
        };

        // This will create an empty database if it doesn't exist.
        state
            .get_sqlite_engine()?
            .application_data_storage()
            .map_err(|e| (PlatformError::StorageError(e.into_any_error())))?;

        Ok(state)
    }

    pub fn get_signing_identities(&self) -> Result<Vec<Identity>, PlatformError> {
        todo!();
    }

    pub fn client(
        &self,
        myself_identifier: &[u8],
        myself_credential: Option<Credential>,
        version: ProtocolVersion,
        config: &ClientConfig,
    ) -> Result<Client<impl MlsConfig>, PlatformError> {
        let crypto_provider = mls_rs_crypto_nss::NssCryptoProvider::default();

        let engine = self
            .get_sqlite_engine()?
            .with_context(myself_identifier.to_vec());

        let mut myself_sig_data = self
            .get_sig_data(myself_identifier)?
            .ok_or(PlatformError::UnavailableSecret)?;

        let myself_credential = if let Some(cred) = myself_credential {
            myself_sig_data.credential = Some(cred.clone());
            self.store_sigdata(myself_identifier, &myself_sig_data)?;

            cred
        } else if let Some(cred) = myself_sig_data.credential {
            cred
        } else {
            return Err(PlatformError::UndefinedIdentity);
        };

        let myself_signing_identity =
            SigningIdentity::new(myself_credential, myself_sig_data.public_key.into());

        let mut builder = mls_rs::client_builder::ClientBuilder::new_sqlite(engine)
            .map_err(|e| PlatformError::StorageError(e.into_any_error()))?
            .crypto_provider(crypto_provider)
            .identity_provider(mls_rs::identity::basic::BasicIdentityProvider)
            .signing_identity(
                myself_signing_identity,
                myself_sig_data.secret_key.into(),
                myself_sig_data.cs.into(),
            )
            .protocol_version(version);

        if let Some(key_package_extensions) = &config.key_package_extensions {
            builder = builder.key_package_extensions(key_package_extensions.clone());
        };

        if let Some(leaf_node_extensions) = &config.leaf_node_extensions {
            builder = builder.leaf_node_extensions(leaf_node_extensions.clone());
        }

        if let Some(key_package_lifetime_s) = config.key_package_lifetime_s {
            builder = builder.key_package_lifetime(key_package_lifetime_s);
        }

        let mls_rules = DefaultMlsRules::new().with_commit_options(
            CommitOptions::default().with_allow_external_commit(config.allow_external_commits),
        );

        builder = builder.mls_rules(mls_rules);

        Ok(builder.build())
    }

    pub fn client_default(
        &self,
        myself_identifier: &[u8],
    ) -> Result<Client<impl MlsConfig>, PlatformError> {
        self.client(
            myself_identifier,
            None,
            ProtocolVersion::MLS_10,
            &Default::default(),
        )
    }

    pub fn insert_sigkey(
        &self,
        myself_sigkey: &SignatureSecretKey,
        myself_pubkey: &SignaturePublicKey,
        cs: CipherSuite,
        identifier: &[u8],
    ) -> Result<(), PlatformError> {
        let signature_data = SignatureData {
            public_key: myself_pubkey.to_vec(),
            cs: *cs,
            secret_key: myself_sigkey.to_vec(),
            credential: None,
        };

        self.store_sigdata(identifier, &signature_data)?;

        Ok(())
    }

    fn store_sigdata(&self, identifier: &[u8], data: &SignatureData) -> Result<(), PlatformError> {
        let data = bincode::serialize(&data)?;
        let engine = self.get_sqlite_engine()?;

        let storage = engine
            .application_data_storage()
            .map_err(|e| PlatformError::StorageError(e.into_any_error()))?;

        storage
            .insert(hex::encode(identifier), data)
            .map_err(|e| PlatformError::StorageError(e.into_any_error()))?;
        Ok(())
    }

    pub fn get_sig_data(
        &self,
        myself_identifier: &[u8],
    ) -> Result<Option<SignatureData>, PlatformError> {
        // TODO: Not clear if the option is needed here, the underlying function needs it.
        let key = myself_identifier.to_vec();
        let engine = self.get_sqlite_engine()?;
        let storage = engine
            .application_data_storage()
            .map_err(|e| PlatformError::StorageError(e.into_any_error()))?;

        storage
            .get(&hex::encode(key))
            .map_err(|e| PlatformError::StorageError(e.into_any_error()))?
            .map_or_else(
                || Ok(None),
                |data| bincode::deserialize(&data).map(Some).map_err(Into::into),
            )
    }

    pub fn get_sqlite_engine(
        &self,
    ) -> Result<SqLiteDataStorageEngine<impl ConnectionStrategy>, PlatformError> {
        let path = Path::new(&self.db_path);
        let file_conn = FileConnectionStrategy::new(path);

        let cipher_config = SqlCipherConfig::new(SqlCipherKey::RawKey(self.db_key));
        let cipher_conn = CipheredConnectionStrategy::new(file_conn, cipher_config);

        SqLiteDataStorageEngine::new(cipher_conn)
            .map_err(|e| PlatformError::StorageError(e.into_any_error()))
    }

    pub fn delete_group(&self, gid: &[u8], identifier: &[u8]) -> Result<(), PlatformError> {
        let storage = self
            .get_sqlite_engine()?
            .with_context(identifier.to_vec())
            .group_state_storage()
            .map_err(|_| PlatformError::InternalError)?;

        // Delete the group
        storage
            .delete_group(gid)
            .map_err(|_| PlatformError::InternalError)
    }

    pub fn delete(db_path: &str) -> Result<(), PlatformError> {
        let path = Path::new(db_path);

        if path.exists() {
            std::fs::remove_file(path)?;
        }

        Ok(())
    }
}

// Dependencies for implementation of InMemoryState

// use crate::GroupConfig;
// use mls_rs::mls_rs_codec::{MlsDecode, MlsEncode};
// use mls_rs::{GroupStateStorage, KeyPackageStorage};
// use mls_rs_core::group::{EpochRecord, GroupState};
// use std::{collections::hash_map::Entry, convert::Infallible};

// impl InMemoryState {
//     pub fn new() -> Self {
//         Default::default()
//     }

//     pub fn to_bytes(&self) -> Result<Vec<u8>, PlatformError> {
//         bincode::serialize(self).map_err(Into::into)
//     }

//     pub fn from_bytes(bytes: &[u8]) -> Result<Self, PlatformError> {
//         bincode::deserialize(bytes).map_err(Into::into)
//     }

//     pub fn client(
//         &self,
//         myself: SigningIdentity,
//         group_config: Option<GroupConfig>,
//     ) -> Result<Client<impl MlsConfig>, PlatformError> {
//         let crypto_provider = mls_rs_crypto_nss::NssCryptoProvider::default();
//         let myself_sigkey = self.get_sigkey(&myself)?;

//         let mut builder = mls_rs::client_builder::ClientBuilder::new()
//             .key_package_repo(self.clone())
//             .group_state_storage(self.clone())
//             .crypto_provider(crypto_provider)
//             .identity_provider(mls_rs::identity::basic::BasicIdentityProvider)
//             .signing_identity(
//                 myself,
//                 myself_sigkey.secret_key.into(),
//                 myself_sigkey.cs.into(),
//             );

//         if let Some(config) = group_config {
//             builder = builder
//                 .key_package_extensions(config.options)
//                 .protocol_version(config.version);
//         }

//         Ok(builder.build())
//     }

//     pub fn insert_sigkey(
//         &mut self,
//         myself_sigkey: &SignatureSecretKey,
//         myself_pubkey: &SignaturePublicKey,
//         cs: CipherSuite,
//         identifier: Identity,
//     ) -> Result<(), PlatformError> {
//         let signature_data = SignatureData {
//             public_key: myself_pubkey.to_vec(),
//             cs: *cs,
//             secret_key: myself_sigkey.to_vec(),
//             credential: None,
//         };

//         self.sigkeys.insert(identifier, signature_data);
//         // TODO: We could return the value to indicate if the key
//         // existed (see the definition of insert).
//         Ok(())
//     }

//     pub fn get_sigkey(&self, myself: &SigningIdentity) -> Result<SignatureData, PlatformError> {
//         let key = myself.mls_encode_to_vec()?;

//         self.sigkeys
//             .get(&key)
//             .cloned()
//             .ok_or(PlatformError::UnavailableSecret)
//     }
// }

// impl GroupStateStorage for TemporaryState {
//     type Error = mls_rs::mls_rs_codec::Error;

//     fn max_epoch_id(&self, group_id: &[u8]) -> Result<Option<u64>, Self::Error> {
//         let group_locked = self.groups.lock().unwrap();

//         Ok(group_locked
//             .get(group_id)
//             .and_then(|group_data| group_data.epoch_data.keys().max().copied()))
//     }

//     fn state<T>(&self, group_id: &[u8]) -> Result<Option<T>, Self::Error>
//     where
//         T: GroupState + MlsDecode,
//     {
//         self.groups
//             .lock()
//             .unwrap()
//             .get(group_id)
//             .map(|v| T::mls_decode(&mut v.state_data.as_slice()))
//             .transpose()
//             .map_err(Into::into)
//     }

//     fn epoch<T>(&self, group_id: &[u8], epoch_id: u64) -> Result<Option<T>, Self::Error>
//     where
//         T: EpochRecord + MlsEncode + MlsDecode,
//     {
//         self.groups
//             .lock()
//             .unwrap()
//             .get(group_id)
//             .and_then(|group_data| group_data.epoch_data.get(&epoch_id))
//             .map(|v| T::mls_decode(&mut &v[..]))
//             .transpose()
//             .map_err(Into::into)
//     }

//     fn write<ST, ET>(
//         &mut self,
//         state: ST,
//         epoch_inserts: Vec<ET>,
//         epoch_updates: Vec<ET>,
//     ) -> Result<(), Self::Error>
//     where
//         ST: GroupState + MlsEncode + MlsDecode + Send + Sync,
//         ET: EpochRecord + MlsEncode + MlsDecode + Send + Sync,
//     {
//         let state_data = state.mls_encode_to_vec()?;
//         let mut states = self.groups.lock().unwrap();

//         let group_data = match states.entry(state.id()) {
//             Entry::Occupied(entry) => {
//                 let data = entry.into_mut();
//                 data.state_data = state_data;
//                 data
//             }
//             Entry::Vacant(entry) => entry.insert(GroupData {
//                 state_data,
//                 epoch_data: Default::default(),
//             }),
//         };

//         epoch_inserts.into_iter().try_for_each(|e| {
//             group_data.epoch_data.insert(e.id(), e.mls_encode_to_vec()?);
//             Ok::<_, Self::Error>(())
//         })?;

//         epoch_updates.into_iter().try_for_each(|e| {
//             if let Some(data) = group_data.epoch_data.get_mut(&e.id()) {
//                 *data = e.mls_encode_to_vec()?;
//             };

//             Ok::<_, Self::Error>(())
//         })?;

//         Ok(())
//     }
// }

// impl KeyPackageStorage for TemporaryState {
//     type Error = Infallible;

//     fn insert(&mut self, id: Vec<u8>, pkg: KeyPackageData) -> Result<(), Self::Error> {
//         let mut states = self.key_packages.lock().unwrap();
//         states.insert(id, pkg);
//         Ok(())
//     }

//     fn get(&self, id: &[u8]) -> Result<Option<KeyPackageData>, Self::Error> {
//         Ok(self.key_packages.lock().unwrap().get(id).cloned())
//     }

//     fn delete(&mut self, id: &[u8]) -> Result<(), Self::Error> {
//         let mut states = self.key_packages.lock().unwrap();
//         states.remove(id);
//         Ok(())
//     }
// }
