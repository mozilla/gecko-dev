// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::ops::Deref;

use super::*;
use crate::hash_reference::HashReference;

#[cfg_attr(
    all(feature = "ffi", not(test)),
    safer_ffi_gen::ffi_type(clone, opaque)
)]
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
/// Unique identifier for a proposal message.
pub struct ProposalRef(HashReference);

impl Deref for ProposalRef {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

#[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
impl ProposalRef {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_content<CS: CipherSuiteProvider>(
        cipher_suite_provider: &CS,
        content: &AuthenticatedContent,
    ) -> Result<Self, MlsError> {
        let bytes = &content.mls_encode_to_vec()?;

        Ok(ProposalRef(
            HashReference::compute(bytes, b"MLS 1.0 Proposal Reference", cipher_suite_provider)
                .await?,
        ))
    }

    pub fn as_slice(&self) -> &[u8] {
        &self.0
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use super::*;
    use crate::group::test_utils::{random_bytes, TEST_GROUP};
    use alloc::boxed::Box;

    impl ProposalRef {
        pub fn new_fake(bytes: Vec<u8>) -> Self {
            Self(bytes.into())
        }
    }

    pub fn auth_content_from_proposal<S>(proposal: Proposal, sender: S) -> AuthenticatedContent
    where
        S: Into<Sender>,
    {
        AuthenticatedContent {
            wire_format: WireFormat::PublicMessage,
            content: FramedContent {
                group_id: TEST_GROUP.to_vec(),
                epoch: 0,
                sender: sender.into(),
                authenticated_data: vec![],
                content: Content::Proposal(Box::new(proposal)),
            },
            auth: FramedContentAuthData {
                signature: MessageSignature::from(random_bytes(128)),
                confirmation_tag: None,
            },
        }
    }
}

#[cfg(test)]
mod test {
    use super::test_utils::auth_content_from_proposal;
    use super::*;
    use crate::{
        crypto::test_utils::{test_cipher_suite_provider, try_test_cipher_suite_provider},
        key_package::test_utils::test_key_package,
        tree_kem::leaf_node::test_utils::get_basic_test_node,
    };
    use alloc::boxed::Box;

    use crate::extension::RequiredCapabilitiesExt;

    #[cfg_attr(coverage_nightly, coverage(off))]
    fn get_test_extension_list() -> ExtensionList {
        let test_extension = RequiredCapabilitiesExt {
            extensions: vec![42.into()],
            proposals: Default::default(),
            credentials: vec![],
        };

        let mut extension_list = ExtensionList::new();
        extension_list.set_from(test_extension).unwrap();

        extension_list
    }

    #[derive(serde::Serialize, serde::Deserialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        input: Vec<u8>,
        #[serde(with = "hex::serde")]
        output: Vec<u8>,
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[cfg_attr(coverage_nightly, coverage(off))]
    async fn generate_proposal_test_cases() -> Vec<TestCase> {
        let mut test_cases = Vec::new();

        for (protocol_version, cipher_suite) in
            ProtocolVersion::all().flat_map(|p| CipherSuite::all().map(move |cs| (p, cs)))
        {
            let sender = LeafIndex(0);

            let add = auth_content_from_proposal(
                Proposal::Add(Box::new(AddProposal {
                    key_package: test_key_package(protocol_version, cipher_suite, "alice").await,
                })),
                sender,
            );

            let update = auth_content_from_proposal(
                Proposal::Update(UpdateProposal {
                    leaf_node: get_basic_test_node(cipher_suite, "foo").await,
                }),
                sender,
            );

            let remove = auth_content_from_proposal(
                Proposal::Remove(RemoveProposal {
                    to_remove: LeafIndex(1),
                }),
                sender,
            );

            let group_context_ext = auth_content_from_proposal(
                Proposal::GroupContextExtensions(get_test_extension_list()),
                sender,
            );

            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

            test_cases.push(TestCase {
                cipher_suite: cipher_suite.into(),
                input: add.mls_encode_to_vec().unwrap(),
                output: ProposalRef::from_content(&cipher_suite_provider, &add)
                    .await
                    .unwrap()
                    .to_vec(),
            });

            test_cases.push(TestCase {
                cipher_suite: cipher_suite.into(),
                input: update.mls_encode_to_vec().unwrap(),
                output: ProposalRef::from_content(&cipher_suite_provider, &update)
                    .await
                    .unwrap()
                    .to_vec(),
            });

            test_cases.push(TestCase {
                cipher_suite: cipher_suite.into(),
                input: remove.mls_encode_to_vec().unwrap(),
                output: ProposalRef::from_content(&cipher_suite_provider, &remove)
                    .await
                    .unwrap()
                    .to_vec(),
            });

            test_cases.push(TestCase {
                cipher_suite: cipher_suite.into(),
                input: group_context_ext.mls_encode_to_vec().unwrap(),
                output: ProposalRef::from_content(&cipher_suite_provider, &group_context_ext)
                    .await
                    .unwrap()
                    .to_vec(),
            });
        }

        test_cases
    }

    #[cfg(mls_build_async)]
    async fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(proposal_ref, generate_proposal_test_cases().await)
    }

    #[cfg(not(mls_build_async))]
    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(proposal_ref, generate_proposal_test_cases())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_proposal_ref() {
        let test_cases = load_test_cases().await;

        for one_case in test_cases {
            let Some(cs_provider) = try_test_cipher_suite_provider(one_case.cipher_suite) else {
                continue;
            };

            let proposal_content =
                AuthenticatedContent::mls_decode(&mut one_case.input.as_slice()).unwrap();

            let proposal_ref = ProposalRef::from_content(&cs_provider, &proposal_content)
                .await
                .unwrap();

            let expected_out = ProposalRef(HashReference::from(one_case.output));

            assert_eq!(expected_out, proposal_ref);
        }
    }
}
