use mls_rs_codec::MlsEncode;
use mls_rs_core::protocol_version::ProtocolVersion;

use crate::{
    cipher_suite::CipherSuite,
    client_builder::{BaseConfig, MlsConfig, WithCryptoProvider, WithIdentityProvider},
    group::{framing::MlsMessage, Group},
    identity::basic::BasicIdentityProvider,
    test_utils::{generate_basic_client, get_test_groups},
};

#[cfg(not(feature = "benchmark_pq_crypto"))]
pub use mls_rs_crypto_openssl::OpensslCryptoProvider as MlsCryptoProvider;

#[cfg(feature = "benchmark_pq_crypto")]
pub use mls_rs_crypto_awslc::AwsLcCryptoProvider as MlsCryptoProvider;

pub type TestClientConfig =
    WithIdentityProvider<BasicIdentityProvider, WithCryptoProvider<MlsCryptoProvider, BaseConfig>>;

#[cfg(not(feature = "benchmark_pq_crypto"))]
pub const BENCH_CIPHER_SUITE: CipherSuite = CipherSuite::CURVE25519_AES128;

#[cfg(feature = "benchmark_pq_crypto")]
pub const BENCH_CIPHER_SUITE: CipherSuite = CipherSuite::ML_KEM_768_X25519;

macro_rules! load_test_case_mls {
    ($name:ident, $generate:expr) => {
        load_test_case_mls!($name, $generate, to_vec_pretty)
    };
    ($name:ident, $generate:expr, $to_json:ident) => {{
        #[cfg(any(target_arch = "wasm32", not(feature = "std")))]
        {
            // Do not remove `async`! (The goal of this line is to remove warnings
            // about `$generate` not being used. Actually calling it will make tests fail.)
            let _ = async { $generate };

            mls_rs_codec::MlsDecode::mls_decode(&mut &include_bytes!(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/test_data/",
                stringify!($name),
                ".mls"
            )))
            .unwrap()
        }

        #[cfg(all(not(target_arch = "wasm32"), feature = "std"))]
        {
            let path = concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/test_data/",
                stringify!($name),
                ".mls"
            );

            if !std::path::Path::new(path).exists() {
                std::fs::write(path, $generate.mls_encode_to_vec().unwrap()).unwrap();
            }

            mls_rs_codec::MlsDecode::mls_decode(&mut std::fs::read(path).unwrap().as_slice())
                .unwrap()
        }
    }};
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn generate_test_cases() -> Vec<MlsMessage> {
    let mut cases = Vec::new();

    for size in [16, 64, 128] {
        let group = get_test_groups(
            ProtocolVersion::MLS_10,
            BENCH_CIPHER_SUITE,
            size,
            None,
            false,
            &MlsCryptoProvider::new(),
        )
        .await
        .pop()
        .unwrap();

        let group_info = group
            .group_info_message_allowing_ext_commit(true)
            .await
            .unwrap();

        cases.push(group_info)
    }

    cases
}

#[derive(Clone)]
pub struct GroupStates<C: MlsConfig> {
    pub sender: Group<C>,
    pub receiver: Group<C>,
}

#[cfg(not(mls_build_async))]
pub fn load_group_states() -> Vec<GroupStates<impl MlsConfig>> {
    #[cfg(not(feature = "benchmark_pq_crypto"))]
    let group_infos: Vec<MlsMessage> =
        load_test_case_mls!(group_state, generate_test_cases(), to_vec);

    #[cfg(feature = "benchmark_pq_crypto")]
    let group_infos: Vec<MlsMessage> =
        load_test_case_mls!(group_state_pq, generate_test_cases(), to_vec);

    group_infos
        .into_iter()
        .map(|info| join_group(BENCH_CIPHER_SUITE, info))
        .collect()
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn join_group(cs: CipherSuite, group_info: MlsMessage) -> GroupStates<impl MlsConfig> {
    let client = generate_basic_client(
        cs,
        ProtocolVersion::MLS_10,
        99999999999,
        None,
        false,
        &MlsCryptoProvider::new(),
        None,
    );

    let mut sender = client.commit_external(group_info).await.unwrap().0;

    let client = generate_basic_client(
        cs,
        ProtocolVersion::MLS_10,
        99999999998,
        None,
        false,
        &MlsCryptoProvider::new(),
        None,
    );

    let group_info = sender
        .group_info_message_allowing_ext_commit(true)
        .await
        .unwrap();

    let (receiver, commit) = client.commit_external(group_info).await.unwrap();
    sender.process_incoming_message(commit).await.unwrap();

    GroupStates { sender, receiver }
}
