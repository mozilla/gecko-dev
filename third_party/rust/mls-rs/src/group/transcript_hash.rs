// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::{crypto::CipherSuiteProvider, error::IntoAnyError};

use crate::{
    client::MlsError,
    group::{framing::FramedContent, MessageSignature},
    WireFormat,
};

use super::{AuthenticatedContent, ConfirmationTag};

#[derive(Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ConfirmedTranscriptHash(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for ConfirmedTranscriptHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("ConfirmedTranscriptHash")
            .fmt(f)
    }
}

impl Deref for ConfirmedTranscriptHash {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for ConfirmedTranscriptHash {
    fn from(value: Vec<u8>) -> Self {
        Self(value)
    }
}

impl ConfirmedTranscriptHash {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn create<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        interim_transcript_hash: &InterimTranscriptHash,
        content: &AuthenticatedContent,
    ) -> Result<Self, MlsError> {
        #[derive(Debug, MlsSize, MlsEncode)]
        struct ConfirmedTranscriptHashInput<'a> {
            wire_format: WireFormat,
            content: &'a FramedContent,
            signature: &'a MessageSignature,
        }

        let input = ConfirmedTranscriptHashInput {
            wire_format: content.wire_format,
            content: &content.content,
            signature: &content.auth.signature,
        };

        let hash_input = [
            interim_transcript_hash.deref(),
            input.mls_encode_to_vec()?.deref(),
        ]
        .concat();

        cipher_suite_provider
            .hash(&hash_input)
            .await
            .map(Into::into)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }
}

#[derive(Clone, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct InterimTranscriptHash(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for InterimTranscriptHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("InterimTranscriptHash")
            .fmt(f)
    }
}

impl Deref for InterimTranscriptHash {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for InterimTranscriptHash {
    fn from(value: Vec<u8>) -> Self {
        Self(value)
    }
}

impl InterimTranscriptHash {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn create<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        confirmed: &ConfirmedTranscriptHash,
        confirmation_tag: &ConfirmationTag,
    ) -> Result<Self, MlsError> {
        #[derive(Debug, MlsSize, MlsEncode)]
        struct InterimTranscriptHashInput<'a> {
            confirmation_tag: &'a ConfirmationTag,
        }

        let input = InterimTranscriptHashInput { confirmation_tag }.mls_encode_to_vec()?;

        cipher_suite_provider
            .hash(&[confirmed.0.deref(), &input].concat())
            .await
            .map(Into::into)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }
}

// Test vectors come from the MLS interop repository and contain a proposal by reference.
#[cfg(feature = "by_ref_proposal")]
#[cfg(test)]
mod tests {
    use alloc::vec::Vec;

    use mls_rs_codec::MlsDecode;

    use crate::{
        crypto::test_utils::try_test_cipher_suite_provider,
        group::{framing::ContentType, message_signature::AuthenticatedContent, transcript_hashes},
    };

    #[cfg(not(mls_build_async))]
    use alloc::{boxed::Box, vec};

    #[cfg(not(mls_build_async))]
    use crate::{
        crypto::test_utils::test_cipher_suite_provider,
        group::{
            confirmation_tag::ConfirmationTag,
            framing::Content,
            proposal::{Proposal, ProposalOrRef, RemoveProposal},
            test_utils::get_test_group_context,
            Commit, LeafIndex, Sender,
        },
        mls_rs_codec::MlsEncode,
        CipherSuite, CipherSuiteProvider, WireFormat,
    };

    #[cfg(not(mls_build_async))]
    use super::{ConfirmedTranscriptHash, InterimTranscriptHash};

    #[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
    struct TestCase {
        pub cipher_suite: u16,

        #[serde(with = "hex::serde")]
        pub confirmation_key: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub authenticated_content: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub interim_transcript_hash_before: Vec<u8>,

        #[serde(with = "hex::serde")]
        pub confirmed_transcript_hash_after: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub interim_transcript_hash_after: Vec<u8>,
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn transcript_hash() {
        let test_cases: Vec<TestCase> =
            load_test_case_json!(interop_transcript_hashes, generate_test_vector());

        for test_case in test_cases.into_iter() {
            let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) else {
                continue;
            };

            let auth_content =
                AuthenticatedContent::mls_decode(&mut &*test_case.authenticated_content).unwrap();

            assert!(auth_content.content.content_type() == ContentType::Commit);

            let conf_key = &test_case.confirmation_key;
            let conf_hash_after = test_case.confirmed_transcript_hash_after.into();
            let conf_tag = auth_content.auth.confirmation_tag.clone().unwrap();

            let matches = conf_tag
                .matches(conf_key, &conf_hash_after, &cs)
                .await
                .unwrap();

            assert!(matches);

            let (expected_interim, expected_conf) = transcript_hashes(
                &cs,
                &test_case.interim_transcript_hash_before.into(),
                &auth_content,
            )
            .await
            .unwrap();

            assert_eq!(*expected_interim, test_case.interim_transcript_hash_after);
            assert_eq!(expected_conf, conf_hash_after);
        }
    }

    #[cfg(not(mls_build_async))]
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_test_vector() -> Vec<TestCase> {
        CipherSuite::all().fold(vec![], |mut test_cases, cs| {
            let cs = test_cipher_suite_provider(cs);

            let context = get_test_group_context(0x3456, cs.cipher_suite());

            let proposal = Proposal::Remove(RemoveProposal {
                to_remove: LeafIndex(1),
            });

            let proposal = ProposalOrRef::Proposal(Box::new(proposal));

            let commit = Commit {
                proposals: vec![proposal],
                path: None,
            };

            let signer = cs.signature_key_generate().unwrap().0;

            let mut auth_content = AuthenticatedContent::new_signed(
                &cs,
                &context,
                Sender::Member(0),
                Content::Commit(alloc::boxed::Box::new(commit)),
                &signer,
                WireFormat::PublicMessage,
                vec![],
            )
            .unwrap();

            let interim_hash_before = cs.random_bytes_vec(cs.kdf_extract_size()).unwrap().into();

            let conf_hash_after =
                ConfirmedTranscriptHash::create(&cs, &interim_hash_before, &auth_content).unwrap();

            let conf_key = cs.random_bytes_vec(cs.kdf_extract_size()).unwrap();
            let conf_tag = ConfirmationTag::create(&conf_key, &conf_hash_after, &cs).unwrap();

            let interim_hash_after =
                InterimTranscriptHash::create(&cs, &conf_hash_after, &conf_tag).unwrap();

            auth_content.auth.confirmation_tag = Some(conf_tag);

            let test_case = TestCase {
                cipher_suite: cs.cipher_suite().into(),

                confirmation_key: conf_key,
                authenticated_content: auth_content.mls_encode_to_vec().unwrap(),
                interim_transcript_hash_before: interim_hash_before.0,

                confirmed_transcript_hash_after: conf_hash_after.0,
                interim_transcript_hash_after: interim_hash_after.0,
            };

            test_cases.push(test_case);
            test_cases
        })
    }

    #[cfg(mls_build_async)]
    fn generate_test_vector() -> Vec<TestCase> {
        panic!("Tests cannot be generated in async mode");
    }
}
