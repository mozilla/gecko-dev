// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;

use mls_rs_core::{
    crypto::{CipherSuite, SignatureSecretKey},
    extension::ExtensionList,
    identity::SigningIdentity,
    protocol_version::ProtocolVersion,
};

use crate::{client::MlsError, Client, Group, MlsMessage};

use super::{
    proposal::ReInitProposal, ClientConfig, ExportedTree, JustPreSharedKeyID, MessageProcessor,
    NewMemberInfo, PreSharedKeyID, PskGroupId, PskSecretInput, ResumptionPSKUsage, ResumptionPsk,
};

struct ResumptionGroupParameters<'a> {
    group_id: &'a [u8],
    cipher_suite: CipherSuite,
    version: ProtocolVersion,
    extensions: &'a ExtensionList,
}

pub struct ReinitClient<C: ClientConfig + Clone> {
    client: Client<C>,
    reinit: ReInitProposal,
    psk_input: PskSecretInput,
}

impl<C> Group<C>
where
    C: ClientConfig + Clone,
{
    /// Create a sub-group from a subset of the current group members.
    ///
    /// Membership within the resulting sub-group is indicated by providing a
    /// key package that produces the same
    /// [identity](crate::IdentityProvider::identity) value
    /// as an existing group member. The identity value of each key package
    /// is determined using the
    /// [`IdentityProvider`](crate::IdentityProvider)
    /// that is currently in use by this group instance.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn branch(
        &self,
        sub_group_id: Vec<u8>,
        new_key_packages: Vec<MlsMessage>,
    ) -> Result<(Group<C>, Vec<MlsMessage>), MlsError> {
        let new_group_params = ResumptionGroupParameters {
            group_id: &sub_group_id,
            cipher_suite: self.cipher_suite(),
            version: self.protocol_version(),
            extensions: &self.group_state().context.extensions,
        };

        resumption_create_group(
            self.config.clone(),
            new_key_packages,
            &new_group_params,
            // TODO investigate if it's worth updating your own signing identity here
            self.current_member_signing_identity()?.clone(),
            self.signer.clone(),
            #[cfg(any(feature = "private_message", feature = "psk"))]
            self.resumption_psk_input(ResumptionPSKUsage::Branch)?,
        )
        .await
    }

    /// Join a subgroup that was created by [`Group::branch`].
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn join_subgroup(
        &self,
        welcome: &MlsMessage,
        tree_data: Option<ExportedTree<'_>>,
    ) -> Result<(Group<C>, NewMemberInfo), MlsError> {
        let expected_new_group_prams = ResumptionGroupParameters {
            group_id: &[],
            cipher_suite: self.cipher_suite(),
            version: self.protocol_version(),
            extensions: &self.group_state().context.extensions,
        };

        resumption_join_group(
            self.config.clone(),
            self.signer.clone(),
            welcome,
            tree_data,
            expected_new_group_prams,
            false,
            self.resumption_psk_input(ResumptionPSKUsage::Branch)?,
        )
        .await
    }

    /// Generate a [`ReinitClient`] that can be used to create or join a new group
    /// that is based on properties defined by a [`ReInitProposal`]
    /// committed in a previously accepted commit. This is the only action available
    /// after accepting such a commit. The old group can no longer be used according to the RFC.
    ///
    /// If the [`ReInitProposal`] changes the ciphersuite, then `new_signer`
    /// and `new_signer_identity` must be set and match the new ciphersuite, as indicated by
    /// [`pending_reinit_ciphersuite`](crate::group::StateUpdate::pending_reinit_ciphersuite)
    /// of the [`StateUpdate`](crate::group::StateUpdate) outputted after processing the
    /// commit to the reinit proposal. The value of [identity](crate::IdentityProvider::identity)
    /// must be the same for `new_signing_identity` and the current identity in use by this
    /// group instance.
    pub fn get_reinit_client(
        self,
        new_signer: Option<SignatureSecretKey>,
        new_signing_identity: Option<SigningIdentity>,
    ) -> Result<ReinitClient<C>, MlsError> {
        let psk_input = self.resumption_psk_input(ResumptionPSKUsage::Reinit)?;

        let new_signing_identity = new_signing_identity
            .map(Ok)
            .unwrap_or_else(|| self.current_member_signing_identity().cloned())?;

        let reinit = self
            .state
            .pending_reinit
            .ok_or(MlsError::PendingReInitNotFound)?;

        let new_signer = match new_signer {
            Some(signer) => signer,
            None => self.signer,
        };

        let client = Client::new(
            self.config,
            Some(new_signer),
            Some((new_signing_identity, reinit.new_cipher_suite())),
            reinit.new_version(),
        );

        Ok(ReinitClient {
            client,
            reinit,
            psk_input,
        })
    }

    fn resumption_psk_input(&self, usage: ResumptionPSKUsage) -> Result<PskSecretInput, MlsError> {
        let psk = self.epoch_secrets.resumption_secret.clone();

        let id = JustPreSharedKeyID::Resumption(ResumptionPsk {
            usage,
            psk_group_id: PskGroupId(self.group_id().to_vec()),
            psk_epoch: self.current_epoch(),
        });

        let id = PreSharedKeyID::new(id, self.cipher_suite_provider())?;
        Ok(PskSecretInput { id, psk })
    }
}

/// A [`Client`] that can be used to create or join a new group
/// that is based on properties defined by a [`ReInitProposal`]
/// committed in a previously accepted commit.
impl<C: ClientConfig + Clone> ReinitClient<C> {
    /// Generate a key package for the new group. The key package can
    /// be used in [`ReinitClient::commit`].
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn generate_key_package(&self) -> Result<MlsMessage, MlsError> {
        self.client.generate_key_package_message().await
    }

    /// Create the new group using new key packages of all group members, possibly
    /// generated by [`ReinitClient::generate_key_package`].
    ///
    /// # Warning
    ///
    /// This function will fail if the number of members in the reinitialized
    /// group is not the same as the prior group roster.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn commit(
        self,
        new_key_packages: Vec<MlsMessage>,
    ) -> Result<(Group<C>, Vec<MlsMessage>), MlsError> {
        let new_group_params = ResumptionGroupParameters {
            group_id: self.reinit.group_id(),
            cipher_suite: self.reinit.new_cipher_suite(),
            version: self.reinit.new_version(),
            extensions: self.reinit.new_group_context_extensions(),
        };

        resumption_create_group(
            self.client.config.clone(),
            new_key_packages,
            &new_group_params,
            // These private fields are created with `Some(x)` by `get_reinit_client`
            self.client.signing_identity.unwrap().0,
            self.client.signer.unwrap(),
            #[cfg(any(feature = "private_message", feature = "psk"))]
            self.psk_input,
        )
        .await
    }

    /// Join a reinitialized group that was created by [`ReinitClient::commit`].
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn join(
        self,
        welcome: &MlsMessage,
        tree_data: Option<ExportedTree<'_>>,
    ) -> Result<(Group<C>, NewMemberInfo), MlsError> {
        let reinit = self.reinit;

        let expected_group_params = ResumptionGroupParameters {
            group_id: reinit.group_id(),
            cipher_suite: reinit.new_cipher_suite(),
            version: reinit.new_version(),
            extensions: reinit.new_group_context_extensions(),
        };

        resumption_join_group(
            self.client.config,
            // This private field is created with `Some(x)` by `get_reinit_client`
            self.client.signer.unwrap(),
            welcome,
            tree_data,
            expected_group_params,
            true,
            self.psk_input,
        )
        .await
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn resumption_create_group<C: ClientConfig + Clone>(
    config: C,
    new_key_packages: Vec<MlsMessage>,
    new_group_params: &ResumptionGroupParameters<'_>,
    signing_identity: SigningIdentity,
    signer: SignatureSecretKey,
    psk_input: PskSecretInput,
) -> Result<(Group<C>, Vec<MlsMessage>), MlsError> {
    // Create a new group with new parameters
    let mut group = Group::new(
        config,
        Some(new_group_params.group_id.to_vec()),
        new_group_params.cipher_suite,
        new_group_params.version,
        signing_identity,
        new_group_params.extensions.clone(),
        signer,
    )
    .await?;

    // Install the resumption psk in the new group
    group.previous_psk = Some(psk_input);

    // Create a commit that adds new key packages and uses the resumption PSK
    let mut commit = group.commit_builder();

    for kp in new_key_packages.into_iter() {
        commit = commit.add_member(kp)?;
    }

    let commit = commit.build().await?;
    group.apply_pending_commit().await?;

    // Uninstall the resumption psk on success (in case of failure, the new group is discarded anyway)
    group.previous_psk = None;

    Ok((group, commit.welcome_messages))
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn resumption_join_group<C: ClientConfig + Clone>(
    config: C,
    signer: SignatureSecretKey,
    welcome: &MlsMessage,
    tree_data: Option<ExportedTree<'_>>,
    expected_new_group_params: ResumptionGroupParameters<'_>,
    verify_group_id: bool,
    psk_input: PskSecretInput,
) -> Result<(Group<C>, NewMemberInfo), MlsError> {
    let psk_input = Some(psk_input);

    let (group, new_member_info) =
        Group::<C>::from_welcome_message(welcome, tree_data, config, signer, psk_input).await?;

    if group.protocol_version() != expected_new_group_params.version {
        Err(MlsError::ProtocolVersionMismatch)
    } else if group.cipher_suite() != expected_new_group_params.cipher_suite {
        Err(MlsError::CipherSuiteMismatch)
    } else if verify_group_id && group.group_id() != expected_new_group_params.group_id {
        Err(MlsError::GroupIdMismatch)
    } else if &group.group_state().context.extensions != expected_new_group_params.extensions {
        Err(MlsError::ReInitExtensionsMismatch)
    } else {
        Ok((group, new_member_info))
    }
}
