// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::framing::Content;
use crate::client::MlsError;
use crate::crypto::SignatureSecretKey;
use crate::group::framing::{ContentType, FramedContent, PublicMessage, Sender, WireFormat};
use crate::group::{ConfirmationTag, GroupContext};
use crate::signer::Signable;
use crate::CipherSuiteProvider;
use alloc::vec;
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::protocol_version::ProtocolVersion;

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct FramedContentAuthData {
    pub signature: MessageSignature,
    pub confirmation_tag: Option<ConfirmationTag>,
}

impl MlsSize for FramedContentAuthData {
    fn mls_encoded_len(&self) -> usize {
        self.signature.mls_encoded_len()
            + self
                .confirmation_tag
                .as_ref()
                .map_or(0, |tag| tag.mls_encoded_len())
    }
}

impl MlsEncode for FramedContentAuthData {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        self.signature.mls_encode(writer)?;

        if let Some(ref tag) = self.confirmation_tag {
            tag.mls_encode(writer)?;
        }

        Ok(())
    }
}

impl FramedContentAuthData {
    pub(crate) fn mls_decode(
        reader: &mut &[u8],
        content_type: ContentType,
    ) -> Result<Self, mls_rs_codec::Error> {
        Ok(FramedContentAuthData {
            signature: MessageSignature::mls_decode(reader)?,
            confirmation_tag: match content_type {
                ContentType::Commit => Some(ConfirmationTag::mls_decode(reader)?),
                #[cfg(feature = "private_message")]
                ContentType::Application => None,
                #[cfg(feature = "by_ref_proposal")]
                ContentType::Proposal => None,
            },
        })
    }
}

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct AuthenticatedContent {
    pub(crate) wire_format: WireFormat,
    pub(crate) content: FramedContent,
    pub(crate) auth: FramedContentAuthData,
}

impl From<PublicMessage> for AuthenticatedContent {
    fn from(p: PublicMessage) -> Self {
        Self {
            wire_format: WireFormat::PublicMessage,
            content: p.content,
            auth: p.auth,
        }
    }
}

impl AuthenticatedContent {
    pub(crate) fn new(
        context: &GroupContext,
        sender: Sender,
        content: Content,
        authenticated_data: Vec<u8>,
        wire_format: WireFormat,
    ) -> AuthenticatedContent {
        AuthenticatedContent {
            wire_format,
            content: FramedContent {
                group_id: context.group_id.clone(),
                epoch: context.epoch,
                sender,
                authenticated_data,
                content,
            },
            auth: FramedContentAuthData {
                signature: MessageSignature::empty(),
                confirmation_tag: None,
            },
        }
    }

    #[inline(never)]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn new_signed<P: CipherSuiteProvider>(
        signature_provider: &P,
        context: &GroupContext,
        sender: Sender,
        content: Content,
        signer: &SignatureSecretKey,
        wire_format: WireFormat,
        authenticated_data: Vec<u8>,
    ) -> Result<AuthenticatedContent, MlsError> {
        // Construct an MlsPlaintext object containing the content
        let mut plaintext =
            AuthenticatedContent::new(context, sender, content, authenticated_data, wire_format);

        let signing_context = MessageSigningContext {
            group_context: Some(context),
            protocol_version: context.protocol_version,
        };

        // Sign the MlsPlaintext using the current epoch's GroupContext as context.
        plaintext
            .sign(signature_provider, signer, &signing_context)
            .await?;

        Ok(plaintext)
    }
}

impl MlsDecode for AuthenticatedContent {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, mls_rs_codec::Error> {
        let wire_format = WireFormat::mls_decode(reader)?;
        let content = FramedContent::mls_decode(reader)?;
        let auth_data = FramedContentAuthData::mls_decode(reader, content.content_type())?;

        Ok(AuthenticatedContent {
            wire_format,
            content,
            auth: auth_data,
        })
    }
}

#[derive(Clone, Debug, PartialEq)]
pub(crate) struct AuthenticatedContentTBS<'a> {
    pub(crate) protocol_version: ProtocolVersion,
    pub(crate) wire_format: WireFormat,
    pub(crate) content: &'a FramedContent,
    pub(crate) context: Option<&'a GroupContext>,
}

impl<'a> MlsSize for AuthenticatedContentTBS<'a> {
    fn mls_encoded_len(&self) -> usize {
        self.protocol_version.mls_encoded_len()
            + self.wire_format.mls_encoded_len()
            + self.content.mls_encoded_len()
            + self.context.as_ref().map_or(0, |ctx| ctx.mls_encoded_len())
    }
}

impl<'a> MlsEncode for AuthenticatedContentTBS<'a> {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        self.protocol_version.mls_encode(writer)?;
        self.wire_format.mls_encode(writer)?;
        self.content.mls_encode(writer)?;

        if let Some(context) = self.context {
            context.mls_encode(writer)?;
        }

        Ok(())
    }
}

impl<'a> AuthenticatedContentTBS<'a> {
    /// The group context must not be `None` when the sender is `Member` or `NewMember`.
    pub(crate) fn from_authenticated_content(
        auth_content: &'a AuthenticatedContent,
        group_context: Option<&'a GroupContext>,
        protocol_version: ProtocolVersion,
    ) -> Self {
        AuthenticatedContentTBS {
            protocol_version,
            wire_format: auth_content.wire_format,
            content: &auth_content.content,
            context: match auth_content.content.sender {
                Sender::Member(_) | Sender::NewMemberCommit => group_context,
                #[cfg(feature = "by_ref_proposal")]
                Sender::External(_) => None,
                #[cfg(feature = "by_ref_proposal")]
                Sender::NewMemberProposal => None,
            },
        }
    }
}

#[derive(Debug)]
pub(crate) struct MessageSigningContext<'a> {
    pub group_context: Option<&'a GroupContext>,
    pub protocol_version: ProtocolVersion,
}

impl<'a> Signable<'a> for AuthenticatedContent {
    const SIGN_LABEL: &'static str = "FramedContentTBS";

    type SigningContext = MessageSigningContext<'a>;

    fn signature(&self) -> &[u8] {
        &self.auth.signature
    }

    fn signable_content(
        &self,
        context: &MessageSigningContext,
    ) -> Result<Vec<u8>, mls_rs_codec::Error> {
        AuthenticatedContentTBS::from_authenticated_content(
            self,
            context.group_context,
            context.protocol_version,
        )
        .mls_encode_to_vec()
    }

    fn write_signature(&mut self, signature: Vec<u8>) {
        self.auth.signature = MessageSignature::from(signature)
    }
}

#[derive(Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct MessageSignature(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for MessageSignature {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("MessageSignature")
            .fmt(f)
    }
}

impl MessageSignature {
    pub(crate) fn empty() -> Self {
        MessageSignature(vec![])
    }
}

impl Deref for MessageSignature {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for MessageSignature {
    fn from(v: Vec<u8>) -> Self {
        MessageSignature(v)
    }
}
