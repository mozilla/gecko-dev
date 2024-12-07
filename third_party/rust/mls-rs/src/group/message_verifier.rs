// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(feature = "by_ref_proposal")]
use alloc::{vec, vec::Vec};

use crate::{
    client::MlsError,
    crypto::SignaturePublicKey,
    group::{GroupContext, PublicMessage, Sender},
    signer::Signable,
    tree_kem::{node::LeafIndex, TreeKemPublic},
    CipherSuiteProvider,
};

#[cfg(feature = "by_ref_proposal")]
use crate::{extension::ExternalSendersExt, identity::SigningIdentity};

use super::{
    key_schedule::KeySchedule,
    message_signature::{AuthenticatedContent, MessageSigningContext},
    state::GroupState,
};

#[cfg(feature = "by_ref_proposal")]
use super::proposal::Proposal;

#[derive(Debug)]
pub(crate) enum SignaturePublicKeysContainer<'a> {
    RatchetTree(&'a TreeKemPublic),
    #[cfg(feature = "private_message")]
    List(&'a [Option<SignaturePublicKey>]),
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn verify_plaintext_authentication<P: CipherSuiteProvider>(
    cipher_suite_provider: &P,
    plaintext: PublicMessage,
    key_schedule: Option<&KeySchedule>,
    self_index: Option<LeafIndex>,
    state: &GroupState,
) -> Result<AuthenticatedContent, MlsError> {
    let tag = plaintext.membership_tag.clone();
    let auth_content = AuthenticatedContent::from(plaintext);
    let context = &state.context;

    #[cfg(feature = "by_ref_proposal")]
    let external_signers = external_signers(context);

    let current_tree = &state.public_tree;

    // Verify the membership tag if needed
    match &auth_content.content.sender {
        Sender::Member(index) => {
            if let Some(key_schedule) = key_schedule {
                let expected_tag = &key_schedule
                    .get_membership_tag(&auth_content, context, cipher_suite_provider)
                    .await?;

                let plaintext_tag = tag.as_ref().ok_or(MlsError::InvalidMembershipTag)?;

                if expected_tag != plaintext_tag {
                    return Err(MlsError::InvalidMembershipTag);
                }
            }

            if self_index == Some(LeafIndex(*index)) {
                return Err(MlsError::CantProcessMessageFromSelf);
            }
        }
        _ => {
            tag.is_none()
                .then_some(())
                .ok_or(MlsError::MembershipTagForNonMember)?;
        }
    }

    // Verify that the signature on the MLSAuthenticatedContent verifies using the public key
    // from the credential stored at the leaf in the tree indicated by the sender field.
    verify_auth_content_signature(
        cipher_suite_provider,
        SignaturePublicKeysContainer::RatchetTree(current_tree),
        context,
        &auth_content,
        #[cfg(feature = "by_ref_proposal")]
        &external_signers,
    )
    .await?;

    Ok(auth_content)
}

#[cfg(feature = "by_ref_proposal")]
fn external_signers(context: &GroupContext) -> Vec<SigningIdentity> {
    context
        .extensions
        .get_as::<ExternalSendersExt>()
        .unwrap_or(None)
        .map_or(vec![], |extern_senders_ext| {
            extern_senders_ext.allowed_senders
        })
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn verify_auth_content_signature<P: CipherSuiteProvider>(
    cipher_suite_provider: &P,
    signature_keys_container: SignaturePublicKeysContainer<'_>,
    context: &GroupContext,
    auth_content: &AuthenticatedContent,
    #[cfg(feature = "by_ref_proposal")] external_signers: &[SigningIdentity],
) -> Result<(), MlsError> {
    let sender_public_key = signing_identity_for_sender(
        signature_keys_container,
        &auth_content.content.sender,
        &auth_content.content.content,
        #[cfg(feature = "by_ref_proposal")]
        external_signers,
    )?;

    let context = MessageSigningContext {
        group_context: Some(context),
        protocol_version: context.protocol_version,
    };

    auth_content
        .verify(cipher_suite_provider, &sender_public_key, &context)
        .await?;

    Ok(())
}

fn signing_identity_for_sender(
    signature_keys_container: SignaturePublicKeysContainer,
    sender: &Sender,
    content: &super::framing::Content,
    #[cfg(feature = "by_ref_proposal")] external_signers: &[SigningIdentity],
) -> Result<SignaturePublicKey, MlsError> {
    match sender {
        Sender::Member(leaf_index) => {
            signing_identity_for_member(signature_keys_container, LeafIndex(*leaf_index))
        }
        #[cfg(feature = "by_ref_proposal")]
        Sender::External(external_key_index) => {
            signing_identity_for_external(*external_key_index, external_signers)
        }
        Sender::NewMemberCommit => signing_identity_for_new_member_commit(content),
        #[cfg(feature = "by_ref_proposal")]
        Sender::NewMemberProposal => signing_identity_for_new_member_proposal(content),
    }
}

fn signing_identity_for_member(
    signature_keys_container: SignaturePublicKeysContainer,
    leaf_index: LeafIndex,
) -> Result<SignaturePublicKey, MlsError> {
    match signature_keys_container {
        SignaturePublicKeysContainer::RatchetTree(tree) => Ok(tree
            .get_leaf_node(leaf_index)?
            .signing_identity
            .signature_key
            .clone()), // TODO: We can probably get rid of this clone
        #[cfg(feature = "private_message")]
        SignaturePublicKeysContainer::List(list) => list
            .get(leaf_index.0 as usize)
            .cloned()
            .flatten()
            .ok_or(MlsError::LeafNotFound(*leaf_index)),
    }
}

#[cfg(feature = "by_ref_proposal")]
fn signing_identity_for_external(
    index: u32,
    external_signers: &[SigningIdentity],
) -> Result<SignaturePublicKey, MlsError> {
    external_signers
        .get(index as usize)
        .map(|spk| spk.signature_key.clone())
        .ok_or(MlsError::UnknownSigningIdentityForExternalSender)
}

fn signing_identity_for_new_member_commit(
    content: &super::framing::Content,
) -> Result<SignaturePublicKey, MlsError> {
    match content {
        super::framing::Content::Commit(commit) => {
            if let Some(path) = &commit.path {
                Ok(path.leaf_node.signing_identity.signature_key.clone())
            } else {
                Err(MlsError::CommitMissingPath)
            }
        }
        #[cfg(any(feature = "private_message", feature = "by_ref_proposal"))]
        _ => Err(MlsError::ExpectedCommitForNewMemberCommit),
    }
}

#[cfg(feature = "by_ref_proposal")]
fn signing_identity_for_new_member_proposal(
    content: &super::framing::Content,
) -> Result<SignaturePublicKey, MlsError> {
    match content {
        super::framing::Content::Proposal(proposal) => {
            if let Proposal::Add(p) = proposal.as_ref() {
                Ok(p.key_package
                    .leaf_node
                    .signing_identity
                    .signature_key
                    .clone())
            } else {
                Err(MlsError::ExpectedAddProposalForNewMemberProposal)
            }
        }
        _ => Err(MlsError::ExpectedAddProposalForNewMemberProposal),
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        client::{
            test_utils::{test_client_with_key_pkg, TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
            MlsError,
        },
        client_builder::test_utils::TestClientConfig,
        crypto::test_utils::test_cipher_suite_provider,
        group::{
            membership_tag::MembershipTag,
            message_signature::{AuthenticatedContent, MessageSignature},
            test_utils::{test_group_custom, TestGroup},
            Group, PublicMessage,
        },
        tree_kem::node::LeafIndex,
    };
    use alloc::vec;
    use assert_matches::assert_matches;

    #[cfg(feature = "by_ref_proposal")]
    use crate::{extension::ExternalSendersExt, ExtensionList};

    #[cfg(feature = "by_ref_proposal")]
    use crate::{
        crypto::SignatureSecretKey,
        group::{
            message_signature::MessageSigningContext,
            proposal::{AddProposal, Proposal, RemoveProposal},
            Content,
        },
        key_package::KeyPackageGeneration,
        signer::Signable,
        WireFormat,
    };

    #[cfg(feature = "by_ref_proposal")]
    use alloc::boxed::Box;

    use crate::group::{
        test_utils::{test_group, test_member},
        Sender,
    };

    #[cfg(feature = "by_ref_proposal")]
    use crate::identity::test_utils::get_test_signing_identity;

    use super::{verify_auth_content_signature, verify_plaintext_authentication};

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn make_signed_plaintext(group: &mut Group<TestClientConfig>) -> PublicMessage {
        group
            .commit(vec![])
            .await
            .unwrap()
            .commit_message
            .into_plaintext()
            .unwrap()
    }

    struct TestEnv {
        alice: TestGroup,
        bob: TestGroup,
    }

    impl TestEnv {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        async fn new() -> Self {
            let mut alice = test_group_custom(
                TEST_PROTOCOL_VERSION,
                TEST_CIPHER_SUITE,
                Default::default(),
                None,
                None,
            )
            .await;

            let (bob_client, bob_key_pkg) =
                test_client_with_key_pkg(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "bob").await;

            let commit_output = alice
                .group
                .commit_builder()
                .add_member(bob_key_pkg)
                .unwrap()
                .build()
                .await
                .unwrap();

            alice.group.apply_pending_commit().await.unwrap();

            let (bob, _) = Group::join(
                &commit_output.welcome_messages[0],
                None,
                bob_client.config,
                bob_client.signer.unwrap(),
            )
            .await
            .unwrap();

            TestEnv {
                alice,
                bob: TestGroup { group: bob },
            }
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn valid_plaintext_is_verified() {
        let mut env = TestEnv::new().await;

        let message = make_signed_plaintext(&mut env.alice.group).await;

        verify_plaintext_authentication(
            &env.bob.group.cipher_suite_provider,
            message,
            Some(&env.bob.group.key_schedule),
            None,
            &env.bob.group.state,
        )
        .await
        .unwrap();
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn valid_auth_content_is_verified() {
        let mut env = TestEnv::new().await;

        let message = AuthenticatedContent::from(make_signed_plaintext(&mut env.alice.group).await);

        verify_auth_content_signature(
            &env.bob.group.cipher_suite_provider,
            super::SignaturePublicKeysContainer::RatchetTree(&env.bob.group.state.public_tree),
            env.bob.group.context(),
            &message,
            #[cfg(feature = "by_ref_proposal")]
            &[],
        )
        .await
        .unwrap();
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn invalid_plaintext_is_not_verified() {
        let mut env = TestEnv::new().await;
        let mut message = make_signed_plaintext(&mut env.alice.group).await;
        message.auth.signature = MessageSignature::from(b"test".to_vec());

        message.membership_tag = env
            .alice
            .group
            .key_schedule
            .get_membership_tag(
                &AuthenticatedContent::from(message.clone()),
                env.alice.group.context(),
                &test_cipher_suite_provider(env.alice.group.cipher_suite()),
            )
            .await
            .unwrap()
            .into();

        let res = verify_plaintext_authentication(
            &env.bob.group.cipher_suite_provider,
            message,
            Some(&env.bob.group.key_schedule),
            None,
            &env.bob.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::InvalidSignature));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn plaintext_from_member_requires_membership_tag() {
        let mut env = TestEnv::new().await;
        let mut message = make_signed_plaintext(&mut env.alice.group).await;
        message.membership_tag = None;

        let res = verify_plaintext_authentication(
            &env.bob.group.cipher_suite_provider,
            message,
            Some(&env.bob.group.key_schedule),
            None,
            &env.bob.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::InvalidMembershipTag));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn plaintext_fails_with_invalid_membership_tag() {
        let mut env = TestEnv::new().await;
        let mut message = make_signed_plaintext(&mut env.alice.group).await;
        message.membership_tag = Some(MembershipTag::from(b"test".to_vec()));

        let res = verify_plaintext_authentication(
            &env.bob.group.cipher_suite_provider,
            message,
            Some(&env.bob.group.key_schedule),
            None,
            &env.bob.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::InvalidMembershipTag));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_new_member_proposal<F>(
        key_pkg_gen: KeyPackageGeneration,
        signer: &SignatureSecretKey,
        test_group: &TestGroup,
        mut edit: F,
    ) -> PublicMessage
    where
        F: FnMut(&mut AuthenticatedContent),
    {
        let mut content = AuthenticatedContent::new_signed(
            &test_group.group.cipher_suite_provider,
            test_group.group.context(),
            Sender::NewMemberProposal,
            Content::Proposal(Box::new(Proposal::Add(Box::new(AddProposal {
                key_package: key_pkg_gen.key_package,
            })))),
            signer,
            WireFormat::PublicMessage,
            vec![],
        )
        .await
        .unwrap();

        edit(&mut content);

        let signing_context = MessageSigningContext {
            group_context: Some(test_group.group.context()),
            protocol_version: test_group.group.protocol_version(),
        };

        content
            .sign(
                &test_group.group.cipher_suite_provider,
                signer,
                &signing_context,
            )
            .await
            .unwrap();

        PublicMessage {
            content: content.content,
            auth: content.auth,
            membership_tag: None,
        }
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn valid_proposal_from_new_member_is_verified() {
        let test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let (key_pkg_gen, signer) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;
        let message = test_new_member_proposal(key_pkg_gen, &signer, &test_group, |_| {}).await;

        verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await
        .unwrap();
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn proposal_from_new_member_must_not_have_membership_tag() {
        let test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let (key_pkg_gen, signer) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;

        let mut message = test_new_member_proposal(key_pkg_gen, &signer, &test_group, |_| {}).await;
        message.membership_tag = Some(MembershipTag::from(vec![]));

        let res = verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::MembershipTagForNonMember));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn new_member_proposal_sender_must_be_add_proposal() {
        let test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let (key_pkg_gen, signer) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;

        let message = test_new_member_proposal(key_pkg_gen, &signer, &test_group, |msg| {
            msg.content.content = Content::Proposal(Box::new(Proposal::Remove(RemoveProposal {
                to_remove: LeafIndex(0),
            })))
        })
        .await;

        let res: Result<AuthenticatedContent, MlsError> = verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::ExpectedAddProposalForNewMemberProposal));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn new_member_commit_must_be_external_commit() {
        let test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let (key_pkg_gen, signer) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;

        let message = test_new_member_proposal(key_pkg_gen, &signer, &test_group, |msg| {
            msg.content.sender = Sender::NewMemberCommit;
        })
        .await;

        let res = verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::ExpectedCommitForNewMemberCommit));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn valid_proposal_from_external_is_verified() {
        let (bob_key_pkg_gen, _) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;

        let (ted_signing, ted_secret) = get_test_signing_identity(TEST_CIPHER_SUITE, b"ted").await;

        let mut test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut extensions = ExtensionList::default();

        extensions
            .set_from(ExternalSendersExt {
                allowed_senders: vec![ted_signing],
            })
            .unwrap();

        test_group
            .group
            .commit_builder()
            .set_group_context_ext(extensions)
            .unwrap()
            .build()
            .await
            .unwrap();

        test_group.group.apply_pending_commit().await.unwrap();

        let message = test_new_member_proposal(bob_key_pkg_gen, &ted_secret, &test_group, |msg| {
            msg.content.sender = Sender::External(0)
        })
        .await;

        verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await
        .unwrap();
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_proposal_must_be_from_valid_sender() {
        let (bob_key_pkg_gen, _) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;
        let (_, ted_secret) = get_test_signing_identity(TEST_CIPHER_SUITE, b"ted").await;
        let test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let message = test_new_member_proposal(bob_key_pkg_gen, &ted_secret, &test_group, |msg| {
            msg.content.sender = Sender::External(0)
        })
        .await;

        let res = verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::UnknownSigningIdentityForExternalSender));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn proposal_from_external_sender_must_not_have_membership_tag() {
        let (bob_key_pkg_gen, _) =
            test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"bob").await;

        let (_, ted_secret) = get_test_signing_identity(TEST_CIPHER_SUITE, b"ted").await;

        let test_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let mut message =
            test_new_member_proposal(bob_key_pkg_gen, &ted_secret, &test_group, |_| {}).await;

        message.membership_tag = Some(MembershipTag::from(vec![]));

        let res = verify_plaintext_authentication(
            &test_group.group.cipher_suite_provider,
            message,
            Some(&test_group.group.key_schedule),
            None,
            &test_group.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::MembershipTagForNonMember));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn plaintext_from_self_fails_verification() {
        let mut env = TestEnv::new().await;

        let message = make_signed_plaintext(&mut env.alice.group).await;

        let res = verify_plaintext_authentication(
            &env.alice.group.cipher_suite_provider,
            message,
            Some(&env.alice.group.key_schedule),
            Some(LeafIndex::new(env.alice.group.current_member_index())),
            &env.alice.group.state,
        )
        .await;

        assert_matches!(res, Err(MlsError::CantProcessMessageFromSelf))
    }
}
