// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::ops::Deref;

use crate::{client::MlsError, tree_kem::node::LeafIndex, KeyPackage, KeyPackageRef};

use super::{Commit, FramedContentAuthData, GroupInfo, MembershipTag, Welcome};

#[cfg(feature = "by_ref_proposal")]
use crate::{group::Proposal, mls_rules::ProposalRef};

use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::{
    crypto::{CipherSuite, CipherSuiteProvider},
    protocol_version::ProtocolVersion,
};
use zeroize::ZeroizeOnDrop;

#[cfg(feature = "private_message")]
use alloc::boxed::Box;

#[cfg(feature = "custom_proposal")]
use crate::group::proposal::{CustomProposal, ProposalOrRef};

#[derive(Copy, Clone, Debug, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[repr(u8)]
pub enum ContentType {
    #[cfg(feature = "private_message")]
    Application = 1u8,
    #[cfg(feature = "by_ref_proposal")]
    Proposal = 2u8,
    Commit = 3u8,
}

impl From<&Content> for ContentType {
    fn from(content: &Content) -> Self {
        match content {
            #[cfg(feature = "private_message")]
            Content::Application(_) => ContentType::Application,
            #[cfg(feature = "by_ref_proposal")]
            Content::Proposal(_) => ContentType::Proposal,
            Content::Commit(_) => ContentType::Commit,
        }
    }
}

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Clone, Copy, Debug, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
#[non_exhaustive]
/// Description of a [`MlsMessage`] sender
pub enum Sender {
    /// Current group member index.
    Member(u32) = 1u8,
    /// An external entity sending a proposal proposal identified by an index
    /// in the current
    /// [`ExternalSendersExt`](crate::extension::ExternalSendersExt) stored in
    /// group context extensions.
    #[cfg(feature = "by_ref_proposal")]
    External(u32) = 2u8,
    /// A new member proposing their own addition to the group.
    #[cfg(feature = "by_ref_proposal")]
    NewMemberProposal = 3u8,
    /// A member sending an external commit.
    NewMemberCommit = 4u8,
}

impl From<LeafIndex> for Sender {
    fn from(leaf_index: LeafIndex) -> Self {
        Sender::Member(*leaf_index)
    }
}

impl From<u32> for Sender {
    fn from(leaf_index: u32) -> Self {
        Sender::Member(leaf_index)
    }
}

#[derive(Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode, ZeroizeOnDrop)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ApplicationData(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for ApplicationData {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("ApplicationData")
            .fmt(f)
    }
}

impl From<Vec<u8>> for ApplicationData {
    fn from(data: Vec<u8>) -> Self {
        Self(data)
    }
}

impl Deref for ApplicationData {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl ApplicationData {
    /// Underlying message content.
    pub fn as_bytes(&self) -> &[u8] {
        &self.0
    }
}

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
pub(crate) enum Content {
    #[cfg(feature = "private_message")]
    Application(ApplicationData) = 1u8,
    #[cfg(feature = "by_ref_proposal")]
    Proposal(alloc::boxed::Box<Proposal>) = 2u8,
    Commit(alloc::boxed::Box<Commit>) = 3u8,
}

impl Content {
    pub fn content_type(&self) -> ContentType {
        self.into()
    }
}

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub(crate) struct PublicMessage {
    pub content: FramedContent,
    pub auth: FramedContentAuthData,
    pub membership_tag: Option<MembershipTag>,
}

impl MlsSize for PublicMessage {
    fn mls_encoded_len(&self) -> usize {
        self.content.mls_encoded_len()
            + self.auth.mls_encoded_len()
            + self
                .membership_tag
                .as_ref()
                .map_or(0, |tag| tag.mls_encoded_len())
    }
}

impl MlsEncode for PublicMessage {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        self.content.mls_encode(writer)?;
        self.auth.mls_encode(writer)?;

        self.membership_tag
            .as_ref()
            .map_or(Ok(()), |tag| tag.mls_encode(writer))
    }
}

impl MlsDecode for PublicMessage {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, mls_rs_codec::Error> {
        let content = FramedContent::mls_decode(reader)?;
        let auth = FramedContentAuthData::mls_decode(reader, content.content_type())?;

        let membership_tag = match content.sender {
            Sender::Member(_) => Some(MembershipTag::mls_decode(reader)?),
            _ => None,
        };

        Ok(Self {
            content,
            auth,
            membership_tag,
        })
    }
}

#[cfg(feature = "private_message")]
#[derive(Clone, Debug, PartialEq)]
pub(crate) struct PrivateMessageContent {
    pub content: Content,
    pub auth: FramedContentAuthData,
}

#[cfg(feature = "private_message")]
impl MlsSize for PrivateMessageContent {
    fn mls_encoded_len(&self) -> usize {
        let content_len_without_type = match &self.content {
            Content::Application(c) => c.mls_encoded_len(),
            #[cfg(feature = "by_ref_proposal")]
            Content::Proposal(c) => c.mls_encoded_len(),
            Content::Commit(c) => c.mls_encoded_len(),
        };

        content_len_without_type + self.auth.mls_encoded_len()
    }
}

#[cfg(feature = "private_message")]
impl MlsEncode for PrivateMessageContent {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        match &self.content {
            Content::Application(c) => c.mls_encode(writer),
            #[cfg(feature = "by_ref_proposal")]
            Content::Proposal(c) => c.mls_encode(writer),
            Content::Commit(c) => c.mls_encode(writer),
        }?;

        self.auth.mls_encode(writer)?;

        Ok(())
    }
}

#[cfg(feature = "private_message")]
impl PrivateMessageContent {
    pub(crate) fn mls_decode(
        reader: &mut &[u8],
        content_type: ContentType,
    ) -> Result<Self, mls_rs_codec::Error> {
        let content = match content_type {
            ContentType::Application => Content::Application(ApplicationData::mls_decode(reader)?),
            #[cfg(feature = "by_ref_proposal")]
            ContentType::Proposal => Content::Proposal(Box::new(Proposal::mls_decode(reader)?)),
            ContentType::Commit => {
                Content::Commit(alloc::boxed::Box::new(Commit::mls_decode(reader)?))
            }
        };

        let auth = FramedContentAuthData::mls_decode(reader, content.content_type())?;

        if reader.iter().any(|&i| i != 0u8) {
            // #[cfg(feature = "std")]
            // return Err(mls_rs_codec::Error::Custom(
            //    "non-zero padding bytes discovered".to_string(),
            // ));

            // #[cfg(not(feature = "std"))]
            return Err(mls_rs_codec::Error::Custom(5));
        }

        Ok(Self { content, auth })
    }
}

#[cfg(feature = "private_message")]
#[derive(Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
pub struct PrivateContentAAD {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub group_id: Vec<u8>,
    pub epoch: u64,
    pub content_type: ContentType,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub authenticated_data: Vec<u8>,
}

#[cfg(feature = "private_message")]
impl Debug for PrivateContentAAD {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PrivateContentAAD")
            .field(
                "group_id",
                &mls_rs_core::debug::pretty_group_id(&self.group_id),
            )
            .field("epoch", &self.epoch)
            .field("content_type", &self.content_type)
            .field(
                "authenticated_data",
                &mls_rs_core::debug::pretty_bytes(&self.authenticated_data),
            )
            .finish()
    }
}

#[cfg(feature = "private_message")]
#[derive(Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct PrivateMessage {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub group_id: Vec<u8>,
    pub epoch: u64,
    pub content_type: ContentType,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub authenticated_data: Vec<u8>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub encrypted_sender_data: Vec<u8>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub ciphertext: Vec<u8>,
}

#[cfg(feature = "private_message")]
impl Debug for PrivateMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PrivateMessage")
            .field(
                "group_id",
                &mls_rs_core::debug::pretty_group_id(&self.group_id),
            )
            .field("epoch", &self.epoch)
            .field("content_type", &self.content_type)
            .field(
                "authenticated_data",
                &mls_rs_core::debug::pretty_bytes(&self.authenticated_data),
            )
            .field(
                "encrypted_sender_data",
                &mls_rs_core::debug::pretty_bytes(&self.encrypted_sender_data),
            )
            .field(
                "ciphertext",
                &mls_rs_core::debug::pretty_bytes(&self.ciphertext),
            )
            .finish()
    }
}

#[cfg(feature = "private_message")]
impl From<&PrivateMessage> for PrivateContentAAD {
    fn from(ciphertext: &PrivateMessage) -> Self {
        Self {
            group_id: ciphertext.group_id.clone(),
            epoch: ciphertext.epoch,
            content_type: ciphertext.content_type,
            authenticated_data: ciphertext.authenticated_data.clone(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     ::safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
/// A MLS protocol message for sending data over the wire.
pub struct MlsMessage {
    pub(crate) version: ProtocolVersion,
    pub(crate) payload: MlsMessagePayload,
}

// #[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
#[allow(dead_code)]
impl MlsMessage {
    pub(crate) fn new(version: ProtocolVersion, payload: MlsMessagePayload) -> MlsMessage {
        Self { version, payload }
    }

    #[inline(always)]
    pub(crate) fn into_plaintext(self) -> Option<PublicMessage> {
        match self.payload {
            MlsMessagePayload::Plain(plaintext) => Some(plaintext),
            _ => None,
        }
    }

    #[cfg(feature = "private_message")]
    #[inline(always)]
    pub(crate) fn into_ciphertext(self) -> Option<PrivateMessage> {
        match self.payload {
            MlsMessagePayload::Cipher(ciphertext) => Some(ciphertext),
            _ => None,
        }
    }

    #[inline(always)]
    pub(crate) fn into_welcome(self) -> Option<Welcome> {
        match self.payload {
            MlsMessagePayload::Welcome(welcome) => Some(welcome),
            _ => None,
        }
    }

    #[inline(always)]
    pub fn into_group_info(self) -> Option<GroupInfo> {
        match self.payload {
            MlsMessagePayload::GroupInfo(info) => Some(info),
            _ => None,
        }
    }

    #[inline(always)]
    pub fn as_group_info(&self) -> Option<&GroupInfo> {
        match &self.payload {
            MlsMessagePayload::GroupInfo(info) => Some(info),
            _ => None,
        }
    }

    #[inline(always)]
    pub fn into_key_package(self) -> Option<KeyPackage> {
        match self.payload {
            MlsMessagePayload::KeyPackage(kp) => Some(kp),
            _ => None,
        }
    }

    /// The wire format value describing the contents of this message.
    pub fn wire_format(&self) -> WireFormat {
        match self.payload {
            MlsMessagePayload::Plain(_) => WireFormat::PublicMessage,
            #[cfg(feature = "private_message")]
            MlsMessagePayload::Cipher(_) => WireFormat::PrivateMessage,
            MlsMessagePayload::Welcome(_) => WireFormat::Welcome,
            MlsMessagePayload::GroupInfo(_) => WireFormat::GroupInfo,
            MlsMessagePayload::KeyPackage(_) => WireFormat::KeyPackage,
        }
    }

    /// The epoch that this message belongs to.
    ///
    /// Returns `None` if the message is [`WireFormat::KeyPackage`]
    /// or [`WireFormat::Welcome`]
    // #[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn epoch(&self) -> Option<u64> {
        match &self.payload {
            MlsMessagePayload::Plain(p) => Some(p.content.epoch),
            #[cfg(feature = "private_message")]
            MlsMessagePayload::Cipher(c) => Some(c.epoch),
            MlsMessagePayload::GroupInfo(gi) => Some(gi.group_context.epoch),
            _ => None,
        }
    }

    // #[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn cipher_suite(&self) -> Option<CipherSuite> {
        match &self.payload {
            MlsMessagePayload::GroupInfo(i) => Some(i.group_context.cipher_suite),
            MlsMessagePayload::Welcome(w) => Some(w.cipher_suite),
            MlsMessagePayload::KeyPackage(k) => Some(k.cipher_suite),
            _ => None,
        }
    }

    pub fn group_id(&self) -> Option<&[u8]> {
        match &self.payload {
            MlsMessagePayload::Plain(p) => Some(&p.content.group_id),
            #[cfg(feature = "private_message")]
            MlsMessagePayload::Cipher(p) => Some(&p.group_id),
            MlsMessagePayload::GroupInfo(p) => Some(&p.group_context.group_id),
            MlsMessagePayload::KeyPackage(_) | MlsMessagePayload::Welcome(_) => None,
        }
    }

    /// Deserialize a message from transport.
    #[inline(never)]
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, MlsError> {
        Self::mls_decode(&mut &*bytes).map_err(Into::into)
    }

    /// Serialize a message for transport.
    pub fn to_bytes(&self) -> Result<Vec<u8>, MlsError> {
        self.mls_encode_to_vec().map_err(Into::into)
    }

    /// If this is a plaintext commit message, return all custom proposals committed by value.
    /// If this is not a plaintext or not a commit, this returns an empty list.
    #[cfg(feature = "custom_proposal")]
    pub fn custom_proposals_by_value(&self) -> Vec<&CustomProposal> {
        match &self.payload {
            MlsMessagePayload::Plain(plaintext) => match &plaintext.content.content {
                Content::Commit(commit) => Self::find_custom_proposals(commit),
                _ => Vec::new(),
            },
            _ => Vec::new(),
        }
    }

    /// If this is a welcome message, return key package references of all members who can
    /// join using this message.
    pub fn welcome_key_package_references(&self) -> Vec<&KeyPackageRef> {
        let MlsMessagePayload::Welcome(welcome) = &self.payload else {
            return Vec::new();
        };

        welcome.secrets.iter().map(|s| &s.new_member).collect()
    }

    /// If this is a key package, return its key package reference.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn key_package_reference<C: CipherSuiteProvider>(
        &self,
        cipher_suite: &C,
    ) -> Result<Option<KeyPackageRef>, MlsError> {
        let MlsMessagePayload::KeyPackage(kp) = &self.payload else {
            return Ok(None);
        };

        kp.to_reference(cipher_suite).await.map(Some)
    }

    /// If this is a plaintext proposal, return the proposal reference that can be matched e.g. with
    /// [`StateUpdate::unused_proposals`](super::StateUpdate::unused_proposals).
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn into_proposal_reference<C: CipherSuiteProvider>(
        self,
        cipher_suite: &C,
    ) -> Result<Option<Vec<u8>>, MlsError> {
        let MlsMessagePayload::Plain(public_message) = self.payload else {
            return Ok(None);
        };

        ProposalRef::from_content(cipher_suite, &public_message.into())
            .await
            .map(|r| Some(r.to_vec()))
    }
}

#[cfg(feature = "custom_proposal")]
impl MlsMessage {
    fn find_custom_proposals(commit: &Commit) -> Vec<&CustomProposal> {
        commit
            .proposals
            .iter()
            .filter_map(|p| match p {
                ProposalOrRef::Proposal(p) => match p.as_ref() {
                    crate::group::Proposal::Custom(p) => Some(p),
                    _ => None,
                },
                _ => None,
            })
            .collect()
    }
}

#[allow(clippy::large_enum_variant)]
#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[repr(u16)]
pub(crate) enum MlsMessagePayload {
    Plain(PublicMessage) = 1u16,
    #[cfg(feature = "private_message")]
    Cipher(PrivateMessage) = 2u16,
    Welcome(Welcome) = 3u16,
    GroupInfo(GroupInfo) = 4u16,
    KeyPackage(KeyPackage) = 5u16,
}

impl From<PublicMessage> for MlsMessagePayload {
    fn from(m: PublicMessage) -> Self {
        Self::Plain(m)
    }
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type)]
#[derive(
    Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd, MlsSize, MlsEncode, MlsDecode,
)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u16)]
#[non_exhaustive]
/// Content description of an [`MlsMessage`]
pub enum WireFormat {
    PublicMessage = 1u16,
    PrivateMessage = 2u16,
    Welcome = 3u16,
    GroupInfo = 4u16,
    KeyPackage = 5u16,
}

#[derive(Clone, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct FramedContent {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    pub group_id: Vec<u8>,
    pub epoch: u64,
    pub sender: Sender,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    pub authenticated_data: Vec<u8>,
    pub content: Content,
}

impl Debug for FramedContent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FramedContent")
            .field(
                "group_id",
                &mls_rs_core::debug::pretty_group_id(&self.group_id),
            )
            .field("epoch", &self.epoch)
            .field("sender", &self.sender)
            .field(
                "authenticated_data",
                &mls_rs_core::debug::pretty_bytes(&self.authenticated_data),
            )
            .field("content", &self.content)
            .finish()
    }
}

impl FramedContent {
    pub fn content_type(&self) -> ContentType {
        self.content.content_type()
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    #[cfg(feature = "private_message")]
    use crate::group::test_utils::random_bytes;

    use crate::group::{AuthenticatedContent, MessageSignature};

    use super::*;

    use alloc::boxed::Box;

    pub(crate) fn get_test_auth_content() -> AuthenticatedContent {
        // This is not a valid commit and should not be validated
        let commit = Commit {
            proposals: Default::default(),
            path: None,
        };

        AuthenticatedContent {
            wire_format: WireFormat::PublicMessage,
            content: FramedContent {
                group_id: Vec::new(),
                epoch: 0,
                sender: Sender::Member(1),
                authenticated_data: Vec::new(),
                content: Content::Commit(Box::new(commit)),
            },
            auth: FramedContentAuthData {
                signature: MessageSignature::empty(),
                confirmation_tag: None,
            },
        }
    }

    #[cfg(feature = "private_message")]
    pub(crate) fn get_test_ciphertext_content() -> PrivateMessageContent {
        PrivateMessageContent {
            content: Content::Application(random_bytes(1024).into()),
            auth: FramedContentAuthData {
                signature: MessageSignature::from(random_bytes(128)),
                confirmation_tag: None,
            },
        }
    }

    impl AsRef<[u8]> for ApplicationData {
        fn as_ref(&self) -> &[u8] {
            &self.0
        }
    }
}

#[cfg(feature = "private_message")]
#[cfg(test)]
mod tests {
    use assert_matches::assert_matches;

    use crate::{
        client::test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        crypto::test_utils::test_cipher_suite_provider,
        group::{
            framing::test_utils::get_test_ciphertext_content,
            proposal_ref::test_utils::auth_content_from_proposal, RemoveProposal,
        },
    };

    use super::*;

    #[test]
    fn test_mls_ciphertext_content_mls_encoding() {
        let ciphertext_content = get_test_ciphertext_content();

        let mut encoded = ciphertext_content.mls_encode_to_vec().unwrap();
        encoded.extend_from_slice(&[0u8; 128]);

        let decoded =
            PrivateMessageContent::mls_decode(&mut &*encoded, (&ciphertext_content.content).into())
                .unwrap();

        assert_eq!(ciphertext_content, decoded);
    }

    #[test]
    fn test_mls_ciphertext_content_non_zero_padding_error() {
        let ciphertext_content = get_test_ciphertext_content();

        let mut encoded = ciphertext_content.mls_encode_to_vec().unwrap();
        encoded.extend_from_slice(&[1u8; 128]);

        let decoded =
            PrivateMessageContent::mls_decode(&mut &*encoded, (&ciphertext_content.content).into());

        assert_matches!(decoded, Err(mls_rs_codec::Error::Custom(_)));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn proposal_ref() {
        let cs = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let test_auth = auth_content_from_proposal(
            Proposal::Remove(RemoveProposal {
                to_remove: LeafIndex(0),
            }),
            Sender::External(0),
        );

        let expected_ref = ProposalRef::from_content(&cs, &test_auth).await.unwrap();

        let test_message = MlsMessage {
            version: TEST_PROTOCOL_VERSION,
            payload: MlsMessagePayload::Plain(PublicMessage {
                content: test_auth.content,
                auth: test_auth.auth,
                membership_tag: Some(cs.mac(&[1, 2, 3], &[1, 2, 3]).await.unwrap().into()),
            }),
        };

        let computed_ref = test_message
            .into_proposal_reference(&cs)
            .await
            .unwrap()
            .unwrap();

        assert_eq!(computed_ref, expected_ref.to_vec());
    }
}
