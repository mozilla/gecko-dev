use std::sync::Mutex;

use mls_rs_core::{
    crypto::{CipherSuiteProvider, CryptoProvider, SignatureSecretKey},
    identity::BasicCredential,
};

use once_cell::sync::Lazy;

use crate::{
    cipher_suite::CipherSuite,
    client::MlsError,
    client_builder::{BaseConfig, WithCryptoProvider, WithIdentityProvider},
    group::{
        framing::{Content, MlsMessage, Sender, WireFormat},
        message_processor::MessageProcessor,
        message_signature::AuthenticatedContent,
        Commit, Group,
    },
    identity::{basic::BasicIdentityProvider, SigningIdentity},
    Client, ExtensionList,
};

#[cfg(awslc)]
pub use mls_rs_crypto_awslc::AwsLcCryptoProvider as MlsCryptoProvider;
#[cfg(not(any(awslc, rustcrypto)))]
pub use mls_rs_crypto_openssl::OpensslCryptoProvider as MlsCryptoProvider;
#[cfg(rustcrypto)]
pub use mls_rs_crypto_rustcrypto::RustCryptoProvider as MlsCryptoProvider;

pub type TestClientConfig =
    WithIdentityProvider<BasicIdentityProvider, WithCryptoProvider<MlsCryptoProvider, BaseConfig>>;

pub static GROUP: Lazy<Mutex<Group<TestClientConfig>>> = Lazy::new(|| Mutex::new(create_group()));

pub fn create_group() -> Group<TestClientConfig> {
    let cipher_suite = CipherSuite::CURVE25519_AES128;
    let alice = make_client(cipher_suite, "alice");
    let bob = make_client(cipher_suite, "bob");

    let mut alice = alice.create_group(ExtensionList::new()).unwrap();

    alice
        .commit_builder()
        .add_member(bob.generate_key_package_message().unwrap())
        .unwrap()
        .build()
        .unwrap();

    alice.apply_pending_commit().unwrap();

    alice
}

pub fn create_fuzz_commit_message(
    group_id: Vec<u8>,
    epoch: u64,
    authenticated_data: Vec<u8>,
) -> Result<MlsMessage, MlsError> {
    let mut group = GROUP.lock().unwrap();

    let mut context = group.context().clone();
    context.group_id = group_id;
    context.epoch = epoch;

    #[cfg(feature = "private_message")]
    let wire_format = WireFormat::PrivateMessage;

    #[cfg(not(feature = "private_message"))]
    let wire_format = WireFormat::PublicMessage;

    let auth_content = AuthenticatedContent::new_signed(
        group.cipher_suite_provider(),
        &context,
        Sender::Member(0),
        Content::Commit(alloc::boxed::Box::new(Commit {
            proposals: Vec::new(),
            path: None,
        })),
        &group.signer,
        wire_format,
        authenticated_data,
    )?;

    group.format_for_wire(auth_content)
}

fn make_client(cipher_suite: CipherSuite, name: &str) -> Client<TestClientConfig> {
    let (secret, signing_identity) = make_identity(cipher_suite, name);

    // TODO : consider fuzzing on encrypted controls (doesn't seem very useful)
    Client::builder()
        .identity_provider(BasicIdentityProvider)
        .crypto_provider(MlsCryptoProvider::default())
        .signing_identity(signing_identity, secret, cipher_suite)
        .build()
}

fn make_identity(cipher_suite: CipherSuite, name: &str) -> (SignatureSecretKey, SigningIdentity) {
    let cipher_suite = MlsCryptoProvider::new()
        .cipher_suite_provider(cipher_suite)
        .unwrap();

    let (secret, public) = cipher_suite.signature_key_generate().unwrap();
    let basic_identity = BasicCredential::new(name.as_bytes().to_vec());
    let signing_identity = SigningIdentity::new(basic_identity.into_credential(), public);

    (secret, signing_identity)
}
