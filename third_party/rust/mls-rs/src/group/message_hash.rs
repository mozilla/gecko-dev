use alloc::vec::Vec;
use core::fmt;
use core::fmt::Debug;

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::crypto::CipherSuiteProvider;

use crate::{client::MlsError, error::IntoAnyError, MlsMessage};

#[derive(Clone, PartialEq, Eq, MlsEncode, MlsDecode, MlsSize, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct MessageHash(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for MessageHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("MessageHash")
            .fmt(f)
    }
}

impl MessageHash {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn compute<CS: CipherSuiteProvider>(
        cs: &CS,
        message: &MlsMessage,
    ) -> Result<Self, MlsError> {
        cs.hash(&message.mls_encode_to_vec()?)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
            .map(Self)
    }
}
