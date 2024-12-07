// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use self::{
    message_key::MessageKey,
    reuse_guard::ReuseGuard,
    sender_data_key::{SenderData, SenderDataAAD, SenderDataKey},
};

use super::{
    epoch::EpochSecrets,
    framing::{ContentType, FramedContent, Sender, WireFormat},
    message_signature::AuthenticatedContent,
    padding::PaddingMode,
    secret_tree::{KeyType, MessageKeyData},
    GroupContext,
};
use crate::{
    client::MlsError,
    tree_kem::node::{LeafIndex, NodeIndex},
};
use mls_rs_codec::MlsEncode;
use mls_rs_core::{crypto::CipherSuiteProvider, error::IntoAnyError};
use zeroize::Zeroizing;

mod message_key;
mod reuse_guard;
mod sender_data_key;

#[cfg(feature = "private_message")]
use super::framing::{PrivateContentAAD, PrivateMessage, PrivateMessageContent};

#[cfg(test)]
pub use sender_data_key::test_utils::*;

pub(crate) trait GroupStateProvider {
    fn group_context(&self) -> &GroupContext;
    fn self_index(&self) -> LeafIndex;
    fn epoch_secrets_mut(&mut self) -> &mut EpochSecrets;
    fn epoch_secrets(&self) -> &EpochSecrets;
}

pub(crate) struct CiphertextProcessor<'a, GS, CP>
where
    GS: GroupStateProvider,
    CP: CipherSuiteProvider,
{
    group_state: &'a mut GS,
    cipher_suite_provider: CP,
}

impl<'a, GS, CP> CiphertextProcessor<'a, GS, CP>
where
    GS: GroupStateProvider,
    CP: CipherSuiteProvider,
{
    pub fn new(
        group_state: &'a mut GS,
        cipher_suite_provider: CP,
    ) -> CiphertextProcessor<'a, GS, CP> {
        Self {
            group_state,
            cipher_suite_provider,
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn next_encryption_key(
        &mut self,
        key_type: KeyType,
    ) -> Result<MessageKeyData, MlsError> {
        let self_index = NodeIndex::from(self.group_state.self_index());

        self.group_state
            .epoch_secrets_mut()
            .secret_tree
            .next_message_key(&self.cipher_suite_provider, self_index, key_type)
            .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn decryption_key(
        &mut self,
        sender: LeafIndex,
        key_type: KeyType,
        generation: u32,
    ) -> Result<MessageKeyData, MlsError> {
        let sender = NodeIndex::from(sender);

        self.group_state
            .epoch_secrets_mut()
            .secret_tree
            .message_key_generation(&self.cipher_suite_provider, sender, key_type, generation)
            .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn seal(
        &mut self,
        auth_content: AuthenticatedContent,
        padding: PaddingMode,
    ) -> Result<PrivateMessage, MlsError> {
        if Sender::Member(*self.group_state.self_index()) != auth_content.content.sender {
            return Err(MlsError::InvalidSender);
        }

        let content_type = ContentType::from(&auth_content.content.content);
        let authenticated_data = auth_content.content.authenticated_data;

        // Build a ciphertext content using the plaintext content and signature
        let private_content = PrivateMessageContent {
            content: auth_content.content.content,
            auth: auth_content.auth,
        };

        // Build ciphertext aad using the plaintext message
        let aad = PrivateContentAAD {
            group_id: auth_content.content.group_id,
            epoch: auth_content.content.epoch,
            content_type,
            authenticated_data: authenticated_data.clone(),
        };

        // Generate a 4 byte reuse guard
        let reuse_guard = ReuseGuard::random(&self.cipher_suite_provider)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        // Grab an encryption key from the current epoch's key schedule
        let key_type = match &content_type {
            ContentType::Application => KeyType::Application,
            _ => KeyType::Handshake,
        };

        let mut serialized_private_content = private_content.mls_encode_to_vec()?;

        // Apply padding to private content based on the current padding mode.
        serialized_private_content.resize(padding.padded_size(serialized_private_content.len()), 0);

        let serialized_private_content = Zeroizing::new(serialized_private_content);

        // Encrypt the ciphertext content using the encryption key and a nonce that is
        // reuse safe by xor the reuse guard with the first 4 bytes
        let self_index = self.group_state.self_index();

        let key_data = self.next_encryption_key(key_type).await?;
        let generation = key_data.generation;

        let ciphertext = MessageKey::new(key_data)
            .encrypt(
                &self.cipher_suite_provider,
                &serialized_private_content,
                &aad.mls_encode_to_vec()?,
                &reuse_guard,
            )
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        // Construct an mls sender data struct using the plaintext sender info, the generation
        // of the key schedule encryption key, and the reuse guard used to encrypt ciphertext
        let sender_data = SenderData {
            sender: self_index,
            generation,
            reuse_guard,
        };

        let sender_data_aad = SenderDataAAD {
            group_id: self.group_state.group_context().group_id.clone(),
            epoch: self.group_state.group_context().epoch,
            content_type,
        };

        // Encrypt the sender data with the derived sender_key and sender_nonce from the current
        // epoch's key schedule
        let sender_data_key = SenderDataKey::new(
            &self.group_state.epoch_secrets().sender_data_secret,
            &ciphertext,
            &self.cipher_suite_provider,
        )
        .await?;

        let encrypted_sender_data = sender_data_key.seal(&sender_data, &sender_data_aad).await?;

        Ok(PrivateMessage {
            group_id: self.group_state.group_context().group_id.clone(),
            epoch: self.group_state.group_context().epoch,
            content_type,
            authenticated_data,
            encrypted_sender_data,
            ciphertext,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn open(
        &mut self,
        ciphertext: &PrivateMessage,
    ) -> Result<AuthenticatedContent, MlsError> {
        // Decrypt the sender data with the derived sender_key and sender_nonce from the message
        // epoch's key schedule
        let sender_data_aad = SenderDataAAD {
            group_id: self.group_state.group_context().group_id.clone(),
            epoch: self.group_state.group_context().epoch,
            content_type: ciphertext.content_type,
        };

        let sender_data_key = SenderDataKey::new(
            &self.group_state.epoch_secrets().sender_data_secret,
            &ciphertext.ciphertext,
            &self.cipher_suite_provider,
        )
        .await?;

        let sender_data = sender_data_key
            .open(&ciphertext.encrypted_sender_data, &sender_data_aad)
            .await?;

        if self.group_state.self_index() == sender_data.sender {
            return Err(MlsError::CantProcessMessageFromSelf);
        }

        // Grab a decryption key from the message epoch's key schedule
        let key_type = match &ciphertext.content_type {
            ContentType::Application => KeyType::Application,
            _ => KeyType::Handshake,
        };

        // Decrypt the content of the message using the grabbed key
        let key = self
            .decryption_key(sender_data.sender, key_type, sender_data.generation)
            .await?;

        let sender = Sender::Member(*sender_data.sender);

        let decrypted_content = MessageKey::new(key)
            .decrypt(
                &self.cipher_suite_provider,
                &ciphertext.ciphertext,
                &PrivateContentAAD::from(ciphertext).mls_encode_to_vec()?,
                &sender_data.reuse_guard,
            )
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        let ciphertext_content =
            PrivateMessageContent::mls_decode(&mut &**decrypted_content, ciphertext.content_type)?;

        // Build the MLS plaintext object and process it
        let auth_content = AuthenticatedContent {
            wire_format: WireFormat::PrivateMessage,
            content: FramedContent {
                group_id: ciphertext.group_id.clone(),
                epoch: ciphertext.epoch,
                sender,
                authenticated_data: ciphertext.authenticated_data.clone(),
                content: ciphertext_content.content,
            },
            auth: ciphertext_content.auth,
        };

        Ok(auth_content)
    }
}

#[cfg(test)]
mod test {
    use crate::{
        cipher_suite::CipherSuite,
        client::test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        crypto::{
            test_utils::{test_cipher_suite_provider, TestCryptoProvider},
            CipherSuiteProvider,
        },
        group::{
            framing::{ApplicationData, Content, Sender, WireFormat},
            message_signature::AuthenticatedContent,
            padding::PaddingMode,
            test_utils::{random_bytes, test_group, TestGroup},
        },
        tree_kem::node::LeafIndex,
    };

    use super::{CiphertextProcessor, GroupStateProvider, MlsError};

    use alloc::vec;
    use assert_matches::assert_matches;

    struct TestData {
        group: TestGroup,
        content: AuthenticatedContent,
    }

    fn test_processor(
        group: &mut TestGroup,
        cipher_suite: CipherSuite,
    ) -> CiphertextProcessor<'_, impl GroupStateProvider, impl CipherSuiteProvider> {
        CiphertextProcessor::new(&mut group.group, test_cipher_suite_provider(cipher_suite))
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_data(cipher_suite: CipherSuite) -> TestData {
        let provider = test_cipher_suite_provider(cipher_suite);

        let group = test_group(TEST_PROTOCOL_VERSION, cipher_suite).await;

        let content = AuthenticatedContent::new_signed(
            &provider,
            group.group.context(),
            Sender::Member(0),
            Content::Application(ApplicationData::from(b"test".to_vec())),
            &group.group.signer,
            WireFormat::PrivateMessage,
            vec![],
        )
        .await
        .unwrap();

        TestData { group, content }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_encrypt_decrypt() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let mut test_data = test_data(cipher_suite).await;
            let mut receiver_group = test_data.group.clone();

            let mut ciphertext_processor = test_processor(&mut test_data.group, cipher_suite);

            let ciphertext = ciphertext_processor
                .seal(test_data.content.clone(), PaddingMode::StepFunction)
                .await
                .unwrap();

            receiver_group.group.private_tree.self_index = LeafIndex::new(1);

            let mut receiver_processor = test_processor(&mut receiver_group, cipher_suite);

            let decrypted = receiver_processor.open(&ciphertext).await.unwrap();

            assert_eq!(decrypted, test_data.content);
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_padding_use() {
        let mut test_data = test_data(TEST_CIPHER_SUITE).await;
        let mut ciphertext_processor = test_processor(&mut test_data.group, TEST_CIPHER_SUITE);

        let ciphertext_step = ciphertext_processor
            .seal(test_data.content.clone(), PaddingMode::StepFunction)
            .await
            .unwrap();

        let ciphertext_no_pad = ciphertext_processor
            .seal(test_data.content.clone(), PaddingMode::None)
            .await
            .unwrap();

        assert!(ciphertext_step.ciphertext.len() > ciphertext_no_pad.ciphertext.len());
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_invalid_sender() {
        let mut test_data = test_data(TEST_CIPHER_SUITE).await;
        test_data.content.content.sender = Sender::Member(3);

        let mut ciphertext_processor = test_processor(&mut test_data.group, TEST_CIPHER_SUITE);

        let res = ciphertext_processor
            .seal(test_data.content, PaddingMode::None)
            .await;

        assert_matches!(res, Err(MlsError::InvalidSender))
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_cant_process_from_self() {
        let mut test_data = test_data(TEST_CIPHER_SUITE).await;

        let mut ciphertext_processor = test_processor(&mut test_data.group, TEST_CIPHER_SUITE);

        let ciphertext = ciphertext_processor
            .seal(test_data.content, PaddingMode::None)
            .await
            .unwrap();

        let res = ciphertext_processor.open(&ciphertext).await;

        assert_matches!(res, Err(MlsError::CantProcessMessageFromSelf))
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_decryption_error() {
        let mut test_data = test_data(TEST_CIPHER_SUITE).await;
        let mut receiver_group = test_data.group.clone();
        let mut ciphertext_processor = test_processor(&mut test_data.group, TEST_CIPHER_SUITE);

        let mut ciphertext = ciphertext_processor
            .seal(test_data.content.clone(), PaddingMode::StepFunction)
            .await
            .unwrap();

        ciphertext.ciphertext = random_bytes(ciphertext.ciphertext.len());
        receiver_group.group.private_tree.self_index = LeafIndex::new(1);

        let res = ciphertext_processor.open(&ciphertext).await;

        assert!(res.is_err());
    }
}
