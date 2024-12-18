// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::crypto::CipherSuiteProvider;
use crate::group::message_signature::{AuthenticatedContentTBS, FramedContentAuthData};
use crate::group::GroupContext;
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;

use super::message_signature::AuthenticatedContent;

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode)]
struct AuthenticatedContentTBM<'a> {
    content_tbs: AuthenticatedContentTBS<'a>,
    auth: &'a FramedContentAuthData,
}

impl<'a> AuthenticatedContentTBM<'a> {
    pub fn from_authenticated_content(
        auth_content: &'a AuthenticatedContent,
        group_context: &'a GroupContext,
    ) -> AuthenticatedContentTBM<'a> {
        AuthenticatedContentTBM {
            content_tbs: AuthenticatedContentTBS::from_authenticated_content(
                auth_content,
                Some(group_context),
                group_context.protocol_version,
            ),
            auth: &auth_content.auth,
        }
    }
}

#[derive(Clone, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
pub struct MembershipTag(#[mls_codec(with = "mls_rs_codec::byte_vec")] Vec<u8>);

impl Debug for MembershipTag {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("MembershipTag")
            .fmt(f)
    }
}

impl Deref for MembershipTag {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for MembershipTag {
    fn from(m: Vec<u8>) -> Self {
        Self(m)
    }
}

impl MembershipTag {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn create<P: CipherSuiteProvider>(
        authenticated_content: &AuthenticatedContent,
        group_context: &GroupContext,
        membership_key: &[u8],
        cipher_suite_provider: &P,
    ) -> Result<Self, MlsError> {
        let plaintext_tbm = AuthenticatedContentTBM::from_authenticated_content(
            authenticated_content,
            group_context,
        );

        let serialized_tbm = plaintext_tbm.mls_encode_to_vec()?;

        let tag = cipher_suite_provider
            .mac(membership_key, &serialized_tbm)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        Ok(MembershipTag(tag))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::crypto::test_utils::{test_cipher_suite_provider, try_test_cipher_suite_provider};
    use crate::group::{
        framing::test_utils::get_test_auth_content, test_utils::get_test_group_context,
    };

    #[cfg(not(mls_build_async))]
    use crate::crypto::test_utils::TestCryptoProvider;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        tag: Vec<u8>,
    }

    #[cfg(not(mls_build_async))]
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_test_cases() -> Vec<TestCase> {
        let mut test_cases = Vec::new();

        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let tag = MembershipTag::create(
                &get_test_auth_content(),
                &get_test_group_context(1, cipher_suite),
                b"membership_key".as_ref(),
                &test_cipher_suite_provider(cipher_suite),
            )
            .unwrap();

            test_cases.push(TestCase {
                cipher_suite: cipher_suite.into(),
                tag: tag.to_vec(),
            });
        }

        test_cases
    }

    #[cfg(mls_build_async)]
    fn generate_test_cases() -> Vec<TestCase> {
        panic!("Tests cannot be generated in async mode");
    }

    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(membership_tag, generate_test_cases())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_membership_tag() {
        for case in load_test_cases() {
            let Some(cs_provider) = try_test_cipher_suite_provider(case.cipher_suite) else {
                continue;
            };

            let tag = MembershipTag::create(
                &get_test_auth_content(),
                &get_test_group_context(1, cs_provider.cipher_suite()).await,
                b"membership_key".as_ref(),
                &test_cipher_suite_provider(cs_provider.cipher_suite()),
            )
            .await
            .unwrap();

            assert_eq!(**tag, case.tag);
        }
    }
}
