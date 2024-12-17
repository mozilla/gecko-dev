// Copyright (c) 2024 Mozilla Corporation and contributors.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

mod state;

use mls_rs::error::{AnyError, IntoAnyError};
use mls_rs::group::proposal::{CustomProposal, ProposalType};
use mls_rs::group::{Capabilities, ExportedTree, ReceivedMessage};
use mls_rs::identity::SigningIdentity;
use mls_rs::mls_rs_codec::{MlsDecode, MlsEncode};
use mls_rs::{CipherSuiteProvider, CryptoProvider, Extension, ExtensionList};

use serde::de::{self, MapAccess, Visitor};
use serde::ser::SerializeStruct;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

pub use state::{PlatformState, TemporaryState};
use std::fmt;

pub type DefaultCryptoProvider = mls_rs_crypto_nss::NssCryptoProvider;
pub type DefaultIdentityProvider = mls_rs::identity::basic::BasicIdentityProvider;

// Re-export the mls_rs types
pub use mls_rs::CipherSuite;
pub use mls_rs::MlsMessage;
pub use mls_rs::ProtocolVersion;

// Define new types
pub type GroupState = Vec<u8>;

pub type MlsGroupId = Vec<u8>;
pub type MlsGroupIdArg<'a> = &'a [u8];

pub type MlsGroupEpoch = u64;

pub type MlsCredential = Vec<u8>;
pub type MlsCredentialArg<'a> = &'a [u8];

pub type Identity = Vec<u8>;
pub type IdentityArg<'a> = &'a [u8];

#[derive(Debug, Clone)]
#[allow(clippy::large_enum_variant)]
pub enum MessageOrAck {
    Ack(MlsGroupId),
    MlsMessage(MlsMessage),
}

///
/// Errors
///
#[derive(Debug, thiserror::Error)]
pub enum PlatformError {
    #[error("CoreError")]
    CoreError,
    #[error(transparent)]
    LibraryError(#[from] mls_rs::error::MlsError),
    #[error("InternalError")]
    InternalError,
    #[error("IdentityError")]
    IdentityError(AnyError),
    #[error("CryptoError")]
    CryptoError(AnyError),
    #[error("UnsupportedCiphersuite")]
    UnsupportedCiphersuite,
    #[error("UnsupportedGroupConfig")]
    UnsupportedGroupConfig,
    #[error("UnsupportedMessage")]
    UnsupportedMessage,
    #[error("UndefinedIdentity")]
    UndefinedIdentity,
    #[error("StorageError")]
    StorageError(AnyError),
    #[error("UnavailableSecret")]
    UnavailableSecret,
    #[error("MutexError")]
    MutexError,
    #[error("JsonConversionError")]
    JsonConversionError,
    #[error(transparent)]
    CodecError(#[from] mls_rs::mls_rs_codec::Error),
    #[error(transparent)]
    BincodeError(#[from] bincode::Error),
    #[error(transparent)]
    IOError(#[from] std::io::Error),
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct GroupIdEpoch {
    pub group_id: MlsGroupId,
    pub group_epoch: MlsGroupEpoch,
}

///
/// Generate or Retrieve a PlatformState.
///
pub fn state_access(name: &str, key: &[u8; 32]) -> Result<PlatformState, PlatformError> {
    PlatformState::new(name, key)
}

///
/// Delete a PlatformState.
///
pub fn state_delete(name: &str) -> Result<(), PlatformError> {
    PlatformState::delete(name)
}

///
/// Delete a specific group in the PlatformState.
///
pub fn state_delete_group(
    state: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
) -> Result<GroupIdEpoch, PlatformError> {
    state.delete_group(gid, myself)?;

    // Return the group id and 0xFF..FF epoch to signal the group is closed
    Ok(GroupIdEpoch {
        group_id: gid.to_vec(),
        group_epoch: 0xFFFFFFFFFFFFFFFF,
    })
}

///
/// Configurations
///

// Possibly temporary, allows to add an option to the config without changing every
// call to client() function
#[derive(Clone, Debug, Default)]
pub struct ClientConfig {
    pub key_package_extensions: Option<ExtensionList>,
    pub leaf_node_extensions: Option<ExtensionList>,
    pub leaf_node_capabilities: Option<Capabilities>,
    pub key_package_lifetime_s: Option<u64>,
    pub allow_external_commits: bool,
}

// Assuming GroupConfig is a struct
#[derive(Debug, Clone)]
pub struct GroupConfig {
    pub ciphersuite: CipherSuite,
    pub version: ProtocolVersion,
    pub options: ExtensionList,
}

impl Default for GroupConfig {
    fn default() -> Self {
        GroupConfig {
            // Set default ciphersuite.
            ciphersuite: CipherSuite::CURVE25519_AES128,
            // Set default protocol version.
            version: ProtocolVersion::MLS_10,
            // Set default options.
            options: ExtensionList::new(),
        }
    }
}

///
/// Generate a credential.
///
pub fn mls_generate_credential_basic(content: &[u8]) -> Result<MlsCredential, PlatformError> {
    let credential =
        mls_rs::identity::basic::BasicCredential::new(content.to_vec()).into_credential();
    let credential_bytes = credential.mls_encode_to_vec()?;
    Ok(credential_bytes)
}

///
/// Generate a Signature Keypair
///
pub fn mls_generate_signature_keypair(
    state: &PlatformState,
    cs: CipherSuite,
    // _randomness: Option<Vec<u8>>,
) -> Result<Vec<u8>, PlatformError> {
    let crypto_provider = DefaultCryptoProvider::default();
    let cipher_suite = crypto_provider
        .cipher_suite_provider(cs)
        .ok_or(PlatformError::UnsupportedCiphersuite)?;

    // Generate a signature key pair.
    let (signature_key, signature_pubkey) = cipher_suite
        .signature_key_generate()
        .map_err(|_| PlatformError::UnsupportedCiphersuite)?;

    let cipher_suite_provider = crypto_provider
        .cipher_suite_provider(cs)
        .ok_or(PlatformError::UnsupportedCiphersuite)?;

    let identifier = cipher_suite_provider
        .hash(&signature_pubkey)
        .map_err(|e| PlatformError::CryptoError(e.into_any_error()))?;

    // Store the signature key pair.
    state.insert_sigkey(&signature_key, &signature_pubkey, cs, &identifier)?;

    Ok(identifier)
}

///
/// Generate a KeyPackage.
///
pub fn mls_generate_key_package(
    state: &PlatformState,
    myself: IdentityArg,
    credential: MlsCredentialArg,
    config: &ClientConfig,
    // _randomness: Option<Vec<u8>>,
) -> Result<MlsMessage, PlatformError> {
    // Decode the Credential
    let mut credential_slice: &[u8] = credential;
    let decoded_cred = mls_rs::identity::Credential::mls_decode(&mut credential_slice)?;

    // Create a client for that state
    let client = state.client(myself, Some(decoded_cred), ProtocolVersion::MLS_10, config)?;

    // Generate a KeyPackage from that client_default
    let key_package = client.generate_key_package_message()?;

    // Result
    Ok(key_package)
}

///
/// Get group members.
///

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct ClientIdentifiers {
    pub identity: Identity,
    pub credential: MlsCredential,
    // TODO: identities: Vec<(Identity, Credential, ExtensionList, Capabilities)>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct GroupMembers {
    pub group_id: MlsGroupId,
    pub group_epoch: u64,
    pub group_members: Vec<ClientIdentifiers>,
}

// Note: The identity is needed because it is allowed to have multiple
//       identities in a group.
pub fn mls_group_members(
    state: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
) -> Result<GroupMembers, PlatformError> {
    let crypto_provider = DefaultCryptoProvider::default();

    let group = state.client_default(myself)?.load_group(gid)?;
    let epoch = group.current_epoch();

    let cipher_suite_provider = crypto_provider
        .cipher_suite_provider(group.cipher_suite())
        .ok_or(PlatformError::UnsupportedCiphersuite)?;

    // Return Vec<(Identity, Credential)>
    let members = group
        .roster()
        .member_identities_iter()
        .map(|identity| {
            Ok(ClientIdentifiers {
                identity: cipher_suite_provider
                    .hash(&identity.signature_key)
                    .map_err(|e| PlatformError::CryptoError(e.into_any_error()))?,
                credential: identity.credential.mls_encode_to_vec()?,
            })
        })
        .collect::<Result<Vec<_>, PlatformError>>()?;

    let members = GroupMembers {
        group_id: gid.to_vec(),
        group_epoch: epoch,
        group_members: members,
    };

    Ok(members)
}

///
/// Group management: Create a Group
///

// Note: We internally set the protocol version to avoid issues with compat
pub fn mls_group_create(
    pstate: &mut PlatformState,
    myself: IdentityArg,
    credential: MlsCredentialArg,
    gid: Option<MlsGroupIdArg>,
    group_context_extensions: Option<ExtensionList>,
    config: &ClientConfig,
) -> Result<GroupIdEpoch, PlatformError> {
    // Build the client
    let mut credential_slice: &[u8] = credential;
    let decoded_cred = mls_rs::identity::Credential::mls_decode(&mut credential_slice)?;

    let client = pstate.client(myself, Some(decoded_cred), ProtocolVersion::MLS_10, config)?;

    // Generate a GroupId if none is provided
    let mut group = match gid {
        Some(gid) => client.create_group_with_id(
            gid.to_vec(),
            group_context_extensions.unwrap_or_default().clone(),
        )?,
        None => client.create_group(group_context_extensions.unwrap_or_default().clone())?,
    };

    // The state needs to be returned or stored somewhere
    group.write_to_storage()?;
    let gid = group.group_id().to_vec();
    let epoch = group.current_epoch();

    // Return
    Ok(GroupIdEpoch {
        group_id: gid,
        group_epoch: epoch,
    })
}

///
/// Group management: Adding a user.
///

#[derive(Clone, Debug, PartialEq)]
pub struct MlsCommitOutput {
    pub commit: MlsMessage,
    pub welcome: Vec<MlsMessage>,
    pub group_info: Option<MlsMessage>,
    pub ratchet_tree: Option<Vec<u8>>,
    // pub unused_proposals: Vec<crate::mls_rules::ProposalInfo<Proposal>>, from mls_rs
    pub identity: Option<Identity>,
}

impl Serialize for MlsCommitOutput {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut state = serializer.serialize_struct("MlsCommitOutput", 4)?;

        // Handle serialization for `commit`
        let commit_bytes = self
            .commit
            .mls_encode_to_vec()
            .map_err(serde::ser::Error::custom)?;
        state.serialize_field("commit", &commit_bytes)?;

        // Handle serialization for `welcome`. Collect into a Result to handle potential errors.
        let welcome_bytes: Result<Vec<_>, _> = self
            .welcome
            .iter()
            .map(|msg| msg.mls_encode_to_vec().map_err(serde::ser::Error::custom))
            .collect();
        // Unwrap the Result here, after all potential errors have been handled.
        state.serialize_field("welcome", &welcome_bytes?)?;

        // Handle serialization for `group_info`
        let group_info_bytes = match self.group_info.as_ref().map(|gi| gi.mls_encode_to_vec()) {
            Some(Ok(bytes)) => Some(bytes),
            Some(Err(e)) => return Err(serde::ser::Error::custom(e)),
            None => None,
        };
        state.serialize_field("group_info", &group_info_bytes)?;

        // Directly serialize `ratchet_tree` as it is already an Option<Vec<u8>>
        state.serialize_field("ratchet_tree", &self.ratchet_tree)?;

        state.end()
    }
}

impl<'de> Deserialize<'de> for MlsCommitOutput {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct MlsCommitOutputVisitor;

        impl<'de> Visitor<'de> for MlsCommitOutputVisitor {
            type Value = MlsCommitOutput;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("struct MlsCommitOutput")
            }

            fn visit_map<V>(self, mut map: V) -> Result<MlsCommitOutput, V::Error>
            where
                V: MapAccess<'de>,
            {
                let mut commit = None;
                let mut welcome = None;
                let mut group_info = None;
                let mut ratchet_tree = None;

                while let Some(key) = map.next_key::<String>()? {
                    match key.as_str() {
                        "commit" => {
                            let value: Vec<u8> = map.next_value()?;
                            commit = Some(
                                MlsMessage::mls_decode(&mut &value[..])
                                    .map_err(de::Error::custom)?,
                            );
                        }
                        "welcome" => {
                            let values: Vec<Vec<u8>> = map.next_value()?;
                            welcome = Some(
                                values
                                    .into_iter()
                                    .map(|v| {
                                        MlsMessage::mls_decode(&mut &v[..])
                                            .map_err(de::Error::custom)
                                    })
                                    .collect::<Result<_, _>>()?,
                            );
                        }
                        "group_info" => {
                            if let Some(value) = map.next_value::<Option<Vec<u8>>>()? {
                                group_info = Some(
                                    MlsMessage::mls_decode(&mut &value[..])
                                        .map_err(de::Error::custom)?,
                                );
                            }
                        }
                        "ratchet_tree" => {
                            ratchet_tree = map.next_value()?;
                        }
                        _ => { /* Ignore unknown fields */ }
                    }
                }

                Ok(MlsCommitOutput {
                    commit: commit.ok_or_else(|| de::Error::missing_field("commit"))?,
                    welcome: welcome.ok_or_else(|| de::Error::missing_field("welcome"))?,
                    group_info,
                    ratchet_tree,
                    identity: None,
                })
            }
        }

        const FIELDS: &[&str] = &["commit", "welcome", "group_info", "ratchet_tree"];
        deserializer.deserialize_struct("MlsCommitOutput", FIELDS, MlsCommitOutputVisitor)
    }
}

pub fn mls_group_add(
    pstate: &mut PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    new_members: Vec<MlsMessage>,
) -> Result<MlsCommitOutput, PlatformError> {
    // Get the group from the state
    let client = pstate.client_default(myself)?;
    let mut group = client.load_group(gid)?;

    let commit_output = new_members
        .into_iter()
        .try_fold(group.commit_builder(), |commit_builder, user| {
            commit_builder.add_member(user)
        })?
        .build()?;

    // We use the default mode which returns only one welcome message
    let welcomes = commit_output.welcome_messages; //.remove(0);

    let commit_output = MlsCommitOutput {
        commit: commit_output.commit_message.clone(),
        welcome: welcomes,
        group_info: commit_output.external_commit_group_info,
        ratchet_tree: None, // TODO: Handle this !
        identity: None,
    };

    // Write the group to the storage
    group.write_to_storage()?;

    Ok(commit_output)
}

pub fn mls_group_propose_add(
    pstate: &mut PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    new_member: MlsMessage,
) -> Result<MlsMessage, PlatformError> {
    let client = pstate.client_default(myself)?;
    let mut group = client.load_group(gid)?;

    let proposal = group.propose_add(new_member, vec![])?;
    group.write_to_storage()?;

    Ok(proposal.clone())
}

// Variant 1: Vec<MlsMessage>
// pub fn mls_group_propose_add(
//     pstate: &mut PlatformState,
//     gid: &MlsGroupId,
//     myself: &Identity,
//     new_members: Vec<MlsMessage>,
// ) -> Result<MlsMessage, PlatformError> {
//     let client = pstate.client_default(myself)?;
//     let mut group = client.load_group(gid)?;

//     let proposals: Result<Vec<_>, _> = new_members
//         .into_iter()
//         .map(|member| group.propose_add(member, vec![]))
//         .collect();
//     let proposals = proposals?;

//     let proposal = proposals.first().unwrap();
//     group.write_to_storage()?;

//     Ok(proposal.clone())
// }

///
/// Group management: Removing a user.
///
pub fn mls_group_remove(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    removed: IdentityArg, // TODO: Make this Vec<Identities>?
) -> Result<MlsCommitOutput, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(gid)?;

    let crypto_provider = DefaultCryptoProvider::default();

    let cipher_suite_provider = crypto_provider
        .cipher_suite_provider(group.cipher_suite())
        .ok_or(PlatformError::UnsupportedCiphersuite)?;

    let removed = group
        .roster()
        .members_iter()
        .find_map(|m| {
            let h = cipher_suite_provider
                .hash(&m.signing_identity.signature_key)
                .ok()?;
            (h == *removed).then_some(m.index)
        })
        .ok_or(PlatformError::UndefinedIdentity)?;
    // Handle separate error message for inability to remove yourself

    let commit = group.commit_builder().remove_member(removed)?.build()?;

    // Write the group to the storage
    group.write_to_storage()?;

    let commit_output = MlsCommitOutput {
        commit: commit.commit_message,
        welcome: commit.welcome_messages,
        group_info: commit.external_commit_group_info,
        ratchet_tree: commit
            .ratchet_tree
            .map(|tree| tree.to_bytes())
            .transpose()?,
        identity: None,
    };

    Ok(commit_output)
}

pub fn mls_group_propose_remove(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    removed: IdentityArg, // TODO: Make this Vec<Identities>?
) -> Result<MlsMessage, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(gid)?;

    let crypto_provider = DefaultCryptoProvider::default();

    let cipher_suite_provider = crypto_provider
        .cipher_suite_provider(group.cipher_suite())
        .ok_or(PlatformError::UnsupportedCiphersuite)?;

    let removed = group
        .roster()
        .members_iter()
        .find_map(|m| {
            let h = cipher_suite_provider
                .hash(&m.signing_identity.signature_key)
                .ok()?;
            (h == *removed).then_some(m.index)
        })
        .ok_or(PlatformError::UndefinedIdentity)?;

    let proposal = group.propose_remove(removed, vec![])?;

    // Remember the proposal
    group.write_to_storage()?;

    Ok(proposal)
}

///
/// Key updates
///

/// TODO: Possibly add a random nonce as an optional parameter.
pub fn mls_group_update(
    pstate: &mut PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    signature_key: Option<&[u8]>,
    credential: Option<MlsCredentialArg>,
    group_context_extensions: Option<ExtensionList>,
    config: &ClientConfig,
) -> Result<MlsCommitOutput, PlatformError> {
    let crypto_provider = DefaultCryptoProvider::default();

    // Propose + Commit
    let decoded_cred = credential
        .as_ref()
        .map(|credential| {
            let mut credential_slice: &[u8] = credential;
            mls_rs::identity::Credential::mls_decode(&mut credential_slice)
        })
        .transpose()?;

    let client = pstate.client(myself, decoded_cred, ProtocolVersion::MLS_10, config)?;
    let mut group = client.load_group(gid)?;

    let cipher_suite_provider = crypto_provider
        .cipher_suite_provider(group.cipher_suite())
        .ok_or(PlatformError::UnsupportedCiphersuite)?;

    let mut commit_builder = group.commit_builder();

    if let Some(group_context_extensions) = group_context_extensions {
        commit_builder = commit_builder.set_group_context_ext(group_context_extensions)?;
    }

    let identity = if let Some((key, cred)) = signature_key.zip(credential) {
        let signature_secret_key = key.to_vec().into();
        let signature_public_key = cipher_suite_provider
            .signature_key_derive_public(&signature_secret_key)
            .map_err(|e| PlatformError::CryptoError(e.into_any_error()))?;

        let mut credential_slice: &[u8] = cred;
        let decoded_cred = mls_rs::identity::Credential::mls_decode(&mut credential_slice)?;
        let signing_identity = SigningIdentity::new(decoded_cred, signature_public_key);

        // Return the identity
        cipher_suite_provider
            .hash(&signing_identity.signature_key)
            .map_err(|e| PlatformError::CryptoError(e.into_any_error()))?
    } else {
        myself.to_vec().into()
    };

    let commit = commit_builder.build()?;

    group.write_to_storage()?;

    let commit_output = MlsCommitOutput {
        commit: commit.commit_message,
        welcome: commit.welcome_messages,
        group_info: commit.external_commit_group_info,
        ratchet_tree: commit
            .ratchet_tree
            .map(|tree| tree.to_bytes())
            .transpose()?,
        identity: Some(identity),
    };
    // Generate the signature keypair
    // Return the signing Identity
    // Hash the signingIdentity to get the Identifier

    Ok(commit_output)
}

///
/// Process Welcome message.
///

pub fn mls_group_join(
    pstate: &PlatformState,
    myself: IdentityArg,
    welcome: &MlsMessage,
    ratchet_tree: Option<ExportedTree<'static>>,
) -> Result<GroupIdEpoch, PlatformError> {
    let client = pstate.client_default(myself)?;
    let (mut group, _info) = client.join_group(ratchet_tree, welcome)?;
    let gid = group.group_id().to_vec();
    let epoch = group.current_epoch();

    // Store the state
    group.write_to_storage()?;

    // Return the group identifier
    Ok(GroupIdEpoch {
        group_id: gid,
        group_epoch: epoch,
    })
}

///
/// Close a group by removing all members.
///

// TODO: Define a custom proposal instead.
pub fn mls_group_close(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
) -> Result<MlsCommitOutput, PlatformError> {
    // Remove everyone from the group.
    let mut group = pstate.client_default(myself)?.load_group(gid)?;
    let self_index = group.current_member_index();

    let all_but_me = group
        .roster()
        .members_iter()
        .filter_map(|m| (m.index != self_index).then_some(m.index))
        .collect::<Vec<_>>();

    let commit_output = all_but_me
        .into_iter()
        .try_fold(group.commit_builder(), |builder, index| {
            builder.remove_member(index)
        })?
        .build()?;

    let commit_output = MlsCommitOutput {
        commit: commit_output.commit_message.clone(),
        welcome: vec![],
        group_info: commit_output.external_commit_group_info,
        ratchet_tree: None, // TODO: Handle this !
        identity: None,
    };
    // TODO we should delete state when we receive an ACK. but it's not super clear how to
    // determine on receive that this was a "close" commit. Would be easier if we had a custom
    // proposal

    // Write the group to the storage
    group.write_to_storage()?;

    Ok(commit_output)
}

///
/// Receive a message
///

#[derive(Clone, Debug, PartialEq)]
#[allow(clippy::large_enum_variant)]
pub enum Received {
    None,
    ApplicationMessage(Vec<u8>),
    GroupIdEpoch(GroupIdEpoch),
    CommitOutput(MlsCommitOutput),
}

pub fn mls_receive(
    pstate: &PlatformState,
    myself: IdentityArg,
    message_or_ack: &MessageOrAck,
) -> Result<(Vec<u8>, Received), PlatformError> {
    // Extract the gid from the Message
    let gid = match &message_or_ack {
        MessageOrAck::Ack(gid) => gid,
        MessageOrAck::MlsMessage(message) => match message.group_id() {
            Some(gid) => gid,
            None => return Err(PlatformError::UnsupportedMessage),
        },
    };

    let mut group = pstate.client_default(myself)?.load_group(gid)?;

    let received_message = match &message_or_ack {
        MessageOrAck::Ack(_) => group.apply_pending_commit().map(ReceivedMessage::Commit),
        MessageOrAck::MlsMessage(message) => group.process_incoming_message(message.clone()),
    };

    //
    let result = match received_message? {
        ReceivedMessage::ApplicationMessage(app_data_description) => Ok((
            gid.to_vec(),
            Received::ApplicationMessage(app_data_description.data().to_vec()),
        )),
        ReceivedMessage::Proposal(_proposal) => {
            // TODO: We inconditionally return the commit for the received proposal
            let commit = group.commit(vec![])?;

            group.write_to_storage()?;

            let commit_output = MlsCommitOutput {
                commit: commit.commit_message,
                welcome: commit.welcome_messages,
                group_info: commit.external_commit_group_info,
                ratchet_tree: commit
                    .ratchet_tree
                    .map(|tree| tree.to_bytes())
                    .transpose()?,
                identity: None,
            };
            Ok((gid.to_vec(), Received::CommitOutput(commit_output)))
        }
        ReceivedMessage::Commit(commit) => {
            // Check if the group is active or not after applying the commit
            if !commit.state_update.is_active() {
                // Delete the group from the state of the client
                pstate.delete_group(gid, myself)?;

                // Return the group id and 0xFF..FF epoch to signal the group is closed
                let group_epoch = GroupIdEpoch {
                    group_id: group.group_id().to_vec(),
                    group_epoch: 0xFFFFFFFFFFFFFFFF,
                };

                Ok((gid.to_vec(), Received::GroupIdEpoch(group_epoch)))
            } else {
                // TODO: Receiving a group_close commit means the sender receiving
                // is left alone in the group. We should be able delete group automatically.
                // As of now, the user calling group_close has to delete group manually.

                // If this is a normal commit, return the affected group and new epoch
                let group_epoch = GroupIdEpoch {
                    group_id: group.group_id().to_vec(),
                    group_epoch: group.current_epoch(),
                };

                Ok((gid.to_vec(), Received::GroupIdEpoch(group_epoch)))
            }
        }
        // TODO: We could make this more user friendly by allowing to
        // pass a Welcome message. KeyPackages should be rejected.
        _ => Err(PlatformError::UnsupportedMessage),
    }?;

    // Write the state to storage
    group.write_to_storage()?;

    Ok(result)
}

pub fn mls_has_pending_commit(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
) -> Result<bool, PlatformError> {
    let group = pstate.client_default(myself)?.load_group(gid)?;
    let result = group.has_pending_commit();
    Ok(result)
}

pub fn mls_clear_pending_commit(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
) -> Result<bool, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(gid)?;
    group.clear_pending_commit();
    group.write_to_storage()?;
    Ok(true)
}

pub fn mls_apply_pending_commit(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
) -> Result<Received, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(gid)?;

    let received_message = group.apply_pending_commit().map(ReceivedMessage::Commit);

    // Check if the group is active or not after applying the commit
    let result = match received_message? {
        ReceivedMessage::Commit(commit) => {
            // Check if the group is active or not after applying the commit
            if !commit.state_update.is_active() {
                // Delete the group from the state of the client
                pstate.delete_group(gid, myself)?;

                // Return the group id and 0xFF..FF epoch to signal the group is closed
                let group_epoch = GroupIdEpoch {
                    group_id: group.group_id().to_vec(),
                    group_epoch: 0xFFFFFFFFFFFFFFFF,
                };

                Ok(Received::GroupIdEpoch(group_epoch))
            } else {
                // TODO: Receiving a group_close commit means the sender receiving
                // is left alone in the group. We should be able delete group automatically.
                // As of now, the user calling group_close has to delete group manually.

                // If this is a normal commit, return the affected group and new epoch
                let group_epoch = GroupIdEpoch {
                    group_id: group.group_id().to_vec(),
                    group_epoch: group.current_epoch(),
                };

                Ok(Received::GroupIdEpoch(group_epoch))
            }
        }
        _ => Err(PlatformError::UnsupportedMessage),
    }?;

    // Write the state to storage
    group.write_to_storage()?;

    Ok(result)
}

//
// Encrypt a message.
//

pub fn mls_send(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    message: &[u8],
) -> Result<MlsMessage, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(gid)?;

    let out = group.encrypt_application_message(message, vec![])?;
    group.write_to_storage()?;

    Ok(out)
}

///
/// Propose + Commit a GroupContextExtension
///
pub fn mls_send_group_context_extension(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    new_gce: Vec<Extension>,
) -> Result<mls_rs::MlsMessage, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(&gid)?;

    let commit = group
        .commit_builder()
        .set_group_context_ext(new_gce.into())?
        .build()?;

    Ok(commit.commit_message)
}

///
/// Create and send a custom proposal.
///
pub fn mls_send_custom_proposal(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    proposal_type: ProposalType,
    data: Vec<u8>,
) -> Result<mls_rs::MlsMessage, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(&gid)?;
    let custom_proposal = CustomProposal::new(proposal_type, data);
    let proposal = group.propose_custom(custom_proposal, vec![])?;

    Ok(proposal)
}

///
/// Export a group secret.
///

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct ExporterOutput {
    pub group_id: MlsGroupId,
    pub group_epoch: MlsGroupEpoch,
    pub label: Vec<u8>,
    pub context: Vec<u8>,
    pub exporter: Vec<u8>,
}

pub fn mls_derive_exporter(
    pstate: &PlatformState,
    gid: MlsGroupIdArg,
    myself: IdentityArg,
    label: &[u8],
    context: &[u8],
    len: u64,
) -> Result<ExporterOutput, PlatformError> {
    let group = pstate.client_default(myself)?.load_group(gid)?;
    let secret = group
        .export_secret(label, context, len.try_into().unwrap())?
        .to_vec();

    // Construct the output object
    let epoch_and_exporter = ExporterOutput {
        group_id: gid.to_vec(),
        group_epoch: group.current_epoch(),
        label: label.to_vec(),
        context: label.to_vec(),
        exporter: secret,
    };

    Ok(epoch_and_exporter)
}

///
/// Join a group using the external commit mechanism
///

#[derive(Clone, Debug, PartialEq)]
pub struct MlsExternalCommitOutput {
    pub gid: MlsGroupId,
    pub external_commit: MlsMessage,
}

impl Serialize for MlsExternalCommitOutput {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut state = serializer.serialize_struct("MlsExternalCommitOutput", 2)?;
        state.serialize_field("gid", &self.gid)?;

        // Handle serialization for `commit`
        let external_commit_bytes = self
            .external_commit
            .mls_encode_to_vec()
            .map_err(serde::ser::Error::custom)?;

        state.serialize_field("external_commit", &external_commit_bytes)?;

        state.end()
    }
}

impl<'de> Deserialize<'de> for MlsExternalCommitOutput {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct MlsExternalCommitOutputVisitor;

        impl<'de> Visitor<'de> for MlsExternalCommitOutputVisitor {
            type Value = MlsExternalCommitOutput;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("struct MlsExternalCommitOutput")
            }

            fn visit_map<V>(self, mut map: V) -> Result<MlsExternalCommitOutput, V::Error>
            where
                V: MapAccess<'de>,
            {
                let mut gid = None;
                let mut external_commit = None;

                while let Some(key) = map.next_key::<String>()? {
                    match key.as_str() {
                        "external_commit" => {
                            let value: Vec<u8> = map.next_value()?;
                            external_commit = Some(
                                MlsMessage::mls_decode(&mut &value[..])
                                    .map_err(de::Error::custom)?,
                            );
                        }
                        "gid" => gid = Some(map.next_value()?),
                        _ => { /* Ignore unknown fields */ }
                    }
                }

                Ok(MlsExternalCommitOutput {
                    gid: gid.ok_or_else(|| de::Error::missing_field("gid"))?,
                    external_commit: external_commit
                        .ok_or_else(|| de::Error::missing_field("external_commit"))?,
                })
            }
        }

        const FIELDS: &[&str] = &["gid", "external_commit"];
        deserializer.deserialize_struct(
            "MlsExternalCommitOutput",
            FIELDS,
            MlsExternalCommitOutputVisitor,
        )
    }
}

pub fn mls_group_external_commit(
    pstate: &PlatformState,
    myself: IdentityArg,
    credential: MlsCredentialArg,
    group_info: &MlsMessage,
    ratchet_tree: Option<ExportedTree<'static>>,
) -> Result<MlsExternalCommitOutput, PlatformError> {
    // Clone the credential to avoid mutating the original
    let mut credential_slice: &[u8] = credential;

    // Decode the credential
    let decoded_cred = mls_rs::identity::Credential::mls_decode(&mut credential_slice)?;

    let client = pstate.client(
        myself,
        Some(decoded_cred),
        ProtocolVersion::MLS_10,
        &ClientConfig::default(),
    )?;

    let mut commit_builder = client.external_commit_builder()?;

    if let Some(ratchet_tree) = ratchet_tree {
        commit_builder = commit_builder.with_tree_data(ratchet_tree);
    }

    let (mut group, external_commit) = commit_builder.build(group_info.clone())?;
    let gid = group.group_id().to_vec();

    // Store the state
    group.write_to_storage()?;

    // Encode the output
    let gid_and_message = MlsExternalCommitOutput {
        gid,
        external_commit,
    };

    Ok(gid_and_message)
}

///
/// Utility functions
///

pub fn mls_get_group_id(message_or_ack: &MessageOrAck) -> Result<Vec<u8>, PlatformError> {
    // Extract the gid from the Message
    let gid = match &message_or_ack {
        MessageOrAck::Ack(gid) => gid,
        MessageOrAck::MlsMessage(message) => match message.group_id() {
            Some(gid) => gid,
            None => return Err(PlatformError::UnsupportedMessage),
        },
    };

    Ok(gid.to_vec())
}

use serde_json::{Error, Value};

// This function takes a JSON string and converts byte arrays into hex strings.
fn convert_bytes_fields_to_hex(input_str: &str) -> Result<String, Error> {
    // Parse the JSON string into a serde_json::Value
    let mut value: Value = serde_json::from_str(input_str)?;

    // Recursive function to process each element
    fn process_element(element: &mut Value) {
        match element {
            Value::Array(ref mut vec) => {
                if vec
                    .iter()
                    .all(|x| matches!(x, Value::Number(n) if n.is_u64()))
                {
                    // Convert all elements to a Vec<u8> if they are numbers
                    let bytes: Vec<u8> = vec
                        .iter()
                        .filter_map(|x| x.as_u64().map(|n| n as u8))
                        .collect();
                    // Check if the conversion makes sense (the length matches)
                    if bytes.len() == vec.len() {
                        *element = Value::String(hex::encode(bytes));
                    } else {
                        vec.iter_mut().for_each(process_element);
                    }
                } else {
                    vec.iter_mut().for_each(process_element);
                }
            }
            Value::Object(ref mut map) => {
                map.values_mut().for_each(process_element);
            }
            _ => {}
        }
    }
    // Process the element and return the new Json string
    process_element(&mut value);
    serde_json::to_string(&value)
}

// This function accepts bytes, converts them to a string, and then processes the string.
pub fn utils_json_bytes_to_string_custom(input_bytes: &[u8]) -> Result<String, PlatformError> {
    // Convert input bytes to a string
    let input_str =
        std::str::from_utf8(input_bytes).map_err(|_| PlatformError::JsonConversionError)?;

    // Call the original function with the decoded string
    convert_bytes_fields_to_hex(input_str).map_err(|_| PlatformError::JsonConversionError)
}
