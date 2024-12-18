// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use mls_rs_core::{
    crypto::CipherSuiteProvider,
    error::IntoAnyError,
    group::GroupStateStorage,
    key_package::KeyPackageStorage,
    psk::{ExternalPskId, PreSharedKey, PreSharedKeyStorage},
};

use crate::{
    client::MlsError,
    group::{epoch::EpochSecrets, state_repo::GroupStateRepository, GroupContext},
    psk::secret::PskSecret,
};

use super::{secret::PskSecretInput, JustPreSharedKeyID, PreSharedKeyID, ResumptionPsk};

pub(crate) struct PskResolver<'a, GS, K, PS>
where
    GS: GroupStateStorage,
    PS: PreSharedKeyStorage,
    K: KeyPackageStorage,
{
    pub group_context: Option<&'a GroupContext>,
    pub current_epoch: Option<&'a EpochSecrets>,
    pub prior_epochs: Option<&'a GroupStateRepository<GS, K>>,
    pub psk_store: &'a PS,
}

impl<GS: GroupStateStorage, K: KeyPackageStorage, PS: PreSharedKeyStorage>
    PskResolver<'_, GS, K, PS>
{
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn resolve_resumption(&self, psk_id: &ResumptionPsk) -> Result<PreSharedKey, MlsError> {
        if let Some(ctx) = self.group_context {
            if ctx.epoch == psk_id.psk_epoch && ctx.group_id == psk_id.psk_group_id.0 {
                let epoch = self.current_epoch.ok_or(MlsError::OldGroupStateNotFound)?;
                return Ok(epoch.resumption_secret.clone());
            }
        }

        #[cfg(feature = "prior_epoch")]
        if let Some(eps) = self.prior_epochs {
            if let Some(psk) = eps.resumption_secret(psk_id).await? {
                return Ok(psk);
            }
        }

        Err(MlsError::OldGroupStateNotFound)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn resolve_external(&self, psk_id: &ExternalPskId) -> Result<PreSharedKey, MlsError> {
        self.psk_store
            .get(psk_id)
            .await
            .map_err(|e| MlsError::PskStoreError(e.into_any_error()))?
            .ok_or(MlsError::MissingRequiredPsk)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn resolve(&self, id: &[PreSharedKeyID]) -> Result<Vec<PskSecretInput>, MlsError> {
        let mut secret_inputs = Vec::new();

        for id in id {
            let psk = match &id.key_id {
                JustPreSharedKeyID::External(external) => self.resolve_external(external).await,
                JustPreSharedKeyID::Resumption(resumption) => {
                    self.resolve_resumption(resumption).await
                }
            }?;

            secret_inputs.push(PskSecretInput {
                id: id.clone(),
                psk,
            })
        }

        Ok(secret_inputs)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn resolve_to_secret<P: CipherSuiteProvider>(
        &self,
        id: &[PreSharedKeyID],
        cipher_suite_provider: &P,
    ) -> Result<PskSecret, MlsError> {
        let psk = self.resolve(id).await?;
        PskSecret::calculate(&psk, cipher_suite_provider).await
    }
}
