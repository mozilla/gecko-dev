// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// The example shows how how to create an MLS extension implementing an access control policy
/// based on the concept of users, similar to
/// https://bifurcation.github.io/ietf-mimi-protocol/draft-ralston-mimi-protocol.html.
///
/// A user, e.g. "bob@b.example", owns zero or more MLS members, e.g. Bob's tablet and PC.
/// Users do not have MLS cryptographic state, while MLS members do. At any point in time,
/// the MLS group has a fixed set of users and for each user, zero or more MLS members they
/// own. Each user also has a role, e.g. a regular user or moderator (which may possibly change
/// over time).
///
/// The goal is to implement the following rule:
/// 1. Each MLS member belongs to a user in the group.
///
/// To this end, we implement the following:
/// * A GroupContext extension containing the current list of users. MLS guarantees agreement
///   on the list.
/// * An AddUser proposal that modifies the user list.
/// * An MLS credential type for MLS members with the owning user's public key and signature.
///   When MLS members join using MLS Add proposals, the signature is verified.
/// * Proposal validation rules that enforce 1. above.
///
use assert_matches::assert_matches;
use mls_rs::{
    client_builder::{MlsConfig, PaddingMode},
    error::MlsError,
    group::{
        proposal::{MlsCustomProposal, Proposal},
        Roster, Sender,
    },
    mls_rules::{
        CommitDirection, CommitOptions, CommitSource, EncryptionOptions, ProposalBundle,
        ProposalSource,
    },
    CipherSuite, CipherSuiteProvider, Client, CryptoProvider, ExtensionList, IdentityProvider,
    MlsRules,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::{
    crypto::{SignaturePublicKey, SignatureSecretKey},
    error::IntoAnyError,
    extension::{ExtensionError, ExtensionType, MlsCodecExtension},
    group::ProposalType,
    identity::{Credential, CredentialType, CustomCredential, MlsCredential, SigningIdentity},
    time::MlsTime,
};

use std::fmt::Display;

const CIPHER_SUITE: CipherSuite = CipherSuite::CURVE25519_AES128;

const ROSTER_EXTENSION_V1: ExtensionType = ExtensionType::new(65000);
const ADD_USER_PROPOSAL_V1: ProposalType = ProposalType::new(65001);
const CREDENTIAL_V1: CredentialType = CredentialType::new(65002);

fn crypto() -> impl CryptoProvider + Clone {
    mls_rs_crypto_openssl::OpensslCryptoProvider::new()
}

fn cipher_suite() -> impl CipherSuiteProvider {
    crypto().cipher_suite_provider(CIPHER_SUITE).unwrap()
}

#[derive(MlsSize, MlsDecode, MlsEncode)]
#[repr(u8)]
enum UserRole {
    Regular = 1u8,
    Moderator = 2u8,
}

#[derive(MlsSize, MlsDecode, MlsEncode)]
struct UserCredential {
    name: String,
    role: UserRole,
    public_key: SignaturePublicKey,
}

#[derive(MlsSize, MlsDecode, MlsEncode)]
struct MemberCredential {
    name: String,
    user_public_key: SignaturePublicKey, // Identifies the user
    signature: Vec<u8>,
}

#[derive(MlsSize, MlsEncode)]
struct MemberCredentialTBS<'a> {
    name: &'a str,
    user_public_key: &'a SignaturePublicKey,
    public_key: &'a SignaturePublicKey,
}

/// The roster will be stored in the custom RosterExtension, an extension in the MLS GroupContext
#[derive(MlsSize, MlsDecode, MlsEncode)]
struct RosterExtension {
    roster: Vec<UserCredential>,
}

impl MlsCodecExtension for RosterExtension {
    fn extension_type() -> ExtensionType {
        ROSTER_EXTENSION_V1
    }
}

/// The custom AddUser proposal will be used to update the RosterExtension
#[derive(MlsSize, MlsDecode, MlsEncode)]
struct AddUserProposal {
    new_user: UserCredential,
}

impl MlsCustomProposal for AddUserProposal {
    fn proposal_type() -> ProposalType {
        ADD_USER_PROPOSAL_V1
    }
}

/// MlsRules tell MLS how to handle our custom proposal
#[derive(Debug, Clone, Copy)]
struct CustomMlsRules;

impl MlsRules for CustomMlsRules {
    type Error = CustomError;

    fn filter_proposals(
        &self,
        _: CommitDirection,
        _: CommitSource,
        _: &Roster,
        extension_list: &ExtensionList,
        mut proposals: ProposalBundle,
    ) -> Result<ProposalBundle, Self::Error> {
        // Find our extension
        let mut roster: RosterExtension =
            extension_list.get_as().ok().flatten().ok_or(CustomError)?;

        // Find AddUser proposals
        let add_user_proposals = proposals
            .custom_proposals()
            .iter()
            .filter(|p| p.proposal.proposal_type() == ADD_USER_PROPOSAL_V1);

        for add_user_info in add_user_proposals {
            let add_user = AddUserProposal::from_custom_proposal(&add_user_info.proposal)?;

            // Eventually we should check for duplicates
            roster.roster.push(add_user.new_user);
        }

        // Issue GroupContextExtensions proposal to modify our roster (eventually we don't have to do this if there were no AddUser proposals)
        let mut new_extensions = extension_list.clone();
        new_extensions.set_from(roster)?;
        let gce_proposal = Proposal::GroupContextExtensions(new_extensions);
        proposals.add(gce_proposal, Sender::Member(0), ProposalSource::Local);

        Ok(proposals)
    }

    fn commit_options(
        &self,
        _: &Roster,
        _: &ExtensionList,
        _: &ProposalBundle,
    ) -> Result<CommitOptions, Self::Error> {
        Ok(CommitOptions::new())
    }

    fn encryption_options(
        &self,
        _: &Roster,
        _: &ExtensionList,
    ) -> Result<EncryptionOptions, Self::Error> {
        Ok(EncryptionOptions::new(false, PaddingMode::None))
    }
}

// The IdentityProvider will tell MLS how to validate members' identities. We will use custom identity
// type to store our User structs.
impl MlsCredential for MemberCredential {
    type Error = CustomError;

    fn credential_type() -> CredentialType {
        CREDENTIAL_V1
    }

    fn into_credential(self) -> Result<Credential, Self::Error> {
        Ok(Credential::Custom(CustomCredential::new(
            Self::credential_type(),
            self.mls_encode_to_vec()?,
        )))
    }
}

#[derive(Debug, Clone, Copy)]
struct CustomIdentityProvider;

impl IdentityProvider for CustomIdentityProvider {
    type Error = CustomError;

    fn validate_member(
        &self,
        signing_identity: &SigningIdentity,
        _: Option<MlsTime>,
        extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error> {
        let Some(extensions) = extensions else {
            return Ok(());
        };

        let roster = extensions
            .get_as::<RosterExtension>()
            .ok()
            .flatten()
            .ok_or(CustomError)?;

        // Retrieve the MemberCredential from the MLS credential
        let Credential::Custom(custom) = &signing_identity.credential else {
            return Err(CustomError);
        };

        if custom.credential_type != CREDENTIAL_V1 {
            return Err(CustomError);
        }

        let member = MemberCredential::mls_decode(&mut &*custom.data)?;

        // Validate the MemberCredential

        let tbs = MemberCredentialTBS {
            name: &member.name,
            user_public_key: &member.user_public_key,
            public_key: &signing_identity.signature_key,
        }
        .mls_encode_to_vec()?;

        cipher_suite()
            .verify(&member.user_public_key, &member.signature, &tbs)
            .map_err(|_| CustomError)?;

        let user_in_roster = roster
            .roster
            .iter()
            .any(|u| u.public_key == member.user_public_key);

        if !user_in_roster {
            return Err(CustomError);
        }

        Ok(())
    }

    fn identity(
        &self,
        signing_identity: &SigningIdentity,
        _: &ExtensionList,
    ) -> Result<Vec<u8>, Self::Error> {
        Ok(signing_identity.mls_encode_to_vec()?)
    }

    fn supported_types(&self) -> Vec<CredentialType> {
        vec![CREDENTIAL_V1]
    }

    fn valid_successor(
        &self,
        _: &SigningIdentity,
        _: &SigningIdentity,
        _: &ExtensionList,
    ) -> Result<bool, Self::Error> {
        Ok(true)
    }

    fn validate_external_sender(
        &self,
        _: &SigningIdentity,
        _: Option<MlsTime>,
        _: Option<&ExtensionList>,
    ) -> Result<(), Self::Error> {
        Ok(())
    }
}

// Convenience structs to create users and members

struct User {
    credential: UserCredential,
    signer: SignatureSecretKey,
}

impl User {
    fn new(name: &str, role: UserRole) -> Result<Self, CustomError> {
        let (signer, public_key) = cipher_suite()
            .signature_key_generate()
            .map_err(|_| CustomError)?;

        let credential = UserCredential {
            name: name.into(),
            role,
            public_key,
        };

        Ok(Self { credential, signer })
    }
}

struct Member {
    credential: MemberCredential,
    public_key: SignaturePublicKey,
    signer: SignatureSecretKey,
}

impl Member {
    fn new(name: &str, user: &User) -> Result<Self, CustomError> {
        let (signer, public_key) = cipher_suite()
            .signature_key_generate()
            .map_err(|_| CustomError)?;

        let tbs = MemberCredentialTBS {
            name,
            user_public_key: &user.credential.public_key,
            public_key: &public_key,
        }
        .mls_encode_to_vec()?;

        let signature = cipher_suite()
            .sign(&user.signer, &tbs)
            .map_err(|_| CustomError)?;

        let credential = MemberCredential {
            name: name.into(),
            user_public_key: user.credential.public_key.clone(),
            signature,
        };

        Ok(Self {
            credential,
            signer,
            public_key,
        })
    }
}

// Set up Client to use our custom providers
fn make_client(member: Member) -> Result<Client<impl MlsConfig>, CustomError> {
    let mls_credential = member.credential.into_credential()?;
    let signing_identity = SigningIdentity::new(mls_credential, member.public_key);

    Ok(Client::builder()
        .identity_provider(CustomIdentityProvider)
        .mls_rules(CustomMlsRules)
        .custom_proposal_type(ADD_USER_PROPOSAL_V1)
        .extension_type(ROSTER_EXTENSION_V1)
        .crypto_provider(crypto())
        .signing_identity(signing_identity, member.signer, CIPHER_SUITE)
        .build())
}

fn main() -> Result<(), CustomError> {
    let alice = User::new("alice", UserRole::Moderator)?;
    let bob = User::new("bob", UserRole::Regular)?;

    let alice_tablet = Member::new("alice tablet", &alice)?;
    let alice_pc = Member::new("alice pc", &alice)?;
    let bob_tablet = Member::new("bob tablet", &bob)?;

    // Alice creates the group with our RosterExtension containing her user
    let mut context_extensions = ExtensionList::new();
    let roster = vec![alice.credential];
    context_extensions.set_from(RosterExtension { roster })?;

    let mut alice_tablet_group = make_client(alice_tablet)?.create_group(context_extensions)?;

    // Alice can add her other device
    let alice_pc_client = make_client(alice_pc)?;
    let key_package = alice_pc_client.generate_key_package_message()?;

    let welcome = alice_tablet_group
        .commit_builder()
        .add_member(key_package)?
        .build()?
        .welcome_messages
        .remove(0);

    alice_tablet_group.apply_pending_commit()?;
    let (mut alice_pc_group, _) = alice_pc_client.join_group(None, &welcome)?;

    // Alice cannot add bob's devices yet
    let bob_tablet_client = make_client(bob_tablet)?;
    let key_package = bob_tablet_client.generate_key_package_message()?;

    let res = alice_tablet_group
        .commit_builder()
        .add_member(key_package.clone())?
        .build();

    assert_matches!(res, Err(MlsError::IdentityProviderError(_)));

    // Alice can add bob's user and device
    let add_bob = AddUserProposal {
        new_user: bob.credential,
    };

    let commit = alice_tablet_group
        .commit_builder()
        .custom_proposal(add_bob.to_custom_proposal()?)
        .add_member(key_package)?
        .build()?;

    bob_tablet_client.join_group(None, &commit.welcome_messages[0])?;
    alice_tablet_group.apply_pending_commit()?;
    alice_pc_group.process_incoming_message(commit.commit_message)?;

    Ok(())
}

#[derive(Debug, thiserror::Error)]
struct CustomError;

impl IntoAnyError for CustomError {
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(Box::new(self))
    }
}

impl Display for CustomError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Custom Error")
    }
}

impl From<MlsError> for CustomError {
    fn from(_: MlsError) -> Self {
        Self
    }
}

impl From<mls_rs_codec::Error> for CustomError {
    fn from(_: mls_rs_codec::Error) -> Self {
        Self
    }
}

impl From<ExtensionError> for CustomError {
    fn from(_: ExtensionError) -> Self {
        Self
    }
}
