// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#![cfg(not(mls_build_async))]

use alloc::vec::Vec;

use crate::{
    context::{Context, ContextR, ContextS, EncryptionContext},
    dhkem::DhKem,
    hpke::Hpke,
};

use mls_rs_core::crypto::test_suite::{EncapOutput, TestHpke};
use mls_rs_crypto_traits::{AeadType, DhType, KdfType, KemResult, KemType};

impl<DH: DhType, KDF: KdfType + Clone, AEAD: AeadType + Clone> TestHpke
    for Hpke<DhKem<DH, KDF>, KDF, AEAD>
{
    type ContextR = ContextR<KDF, AEAD>;
    type ContextS = ContextS<KDF, AEAD>;

    fn hpke_context(
        &self,
        key: Vec<u8>,
        base_nonce: Vec<u8>,
        exporter_secret: Vec<u8>,
    ) -> (Self::ContextS, Self::ContextR) {
        let encryption_context =
            EncryptionContext::new(base_nonce, self.aead.clone().unwrap(), key).unwrap();

        let context = Context::new(
            Some(encryption_context),
            exporter_secret.clone(),
            self.kdf.clone(),
        );

        (ContextS(context.clone()), ContextR(context))
    }

    fn encap(&mut self, ikm_e: Vec<u8>, pk_rm: Vec<u8>) -> EncapOutput {
        self.kem.set_test_data(ikm_e);
        let KemResult { enc, shared_secret } = self.kem.encap(&pk_rm.into()).unwrap();

        EncapOutput { enc, shared_secret }
    }

    fn decap(&mut self, enc: Vec<u8>, sk_rm: Vec<u8>, pk_rm: Vec<u8>) -> Vec<u8> {
        self.kem.decap(&enc, &sk_rm.into(), &pk_rm.into()).unwrap()
    }
}
