// Copyright (c) 2024 Mozilla Corporation and contributors.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

///
/// Get the current epoch.
///

pub type GroupContext = Vec<u8>;

pub fn mls_group_context(
    _state: &PlatformState,
    _gid: &GroupId,
    _myself: &Identity,
) -> Result<GroupContext, PlatformError> {
    unimplemented!()
    // return Json(GroupContext {
    // ...
    // });
}

// TODO: Expose auditable
pub struct PendingJoinState {
    identifier: Vec<u8>,
}

pub fn mls_group_process_welcome(
    pstate: &PlatformState,
    myself: &Identity,
    welcome: MlsMessage,
    ratchet_tree: Option<ExportedTree<'static>>,
) -> Result<PendingJoinState, PlatformError> {
    let client = pstate.client_default(myself)?;
    let (mut group, _info) = client.join_group(ratchet_tree, welcome)?;
    let gid = group.group_id().to_vec();

    // Store the state
    group.write_to_storage()?;

    // Return the group identifier
    Ok(gid)
}

pub fn mls_group_inspect_welcome(
    pstate: &PlatformState,
    myself: &Identity,
    welcome: MlsMessage,
    ratchet_tree: Option<ExportedTree<'static>>,
) -> Result<PendingJoinState, PlatformError> {
    unimplemented!()
}

///
/// Leave a group.
///
pub fn mls_group_propose_leave(
    pstate: PlatformState,
    gid: GroupId,
    myself: &Identity,
) -> Result<mls_rs::MlsMessage, PlatformError> {
    let mut group = pstate.client_default(myself)?.load_group(&gid)?;
    let self_index = group.current_member_index();
    let proposal = group.propose_remove(self_index, vec![])?;

    Ok(proposal)
}

///
/// Import a group state into the storage
///
pub fn mls_import_group_state(
    group_state: Vec<u8>,
    signature_key: SignatureKeypair,
    myself: SigningIdentity,
) -> Result<(), MlsError> {
    unimplemented!()
    // https://github.com/awslabs/mls-rs/blob/main/mls-rs/src/client.rs#L605
}

///
/// Export a group state into the storage
///
pub fn mls_export_group_state(gid: GroupId) -> Result<Vec<u8>, MlsError> {
    unimplemented!()
}

///
/// Validate a KeyPackage.
///

pub fn validate_key_package(key_package: KeyPackage) -> Result<(), MlsError> {
    unimplemented!()
}

///
/// Extract Signing Identity from a KeyPackage.
///

pub fn identity_from_key_package(key_package: KeyPackage) -> Result<SigningIdentity, MlsError> {
    unimplemented!()
}

/// Group configuration.
///
/// This is more or less only for Ciphersuites and GroupContext extensions
/// V8 - Update group context extensions
/// Discuss: do we want to pass the version number explicitly to avoid compat problems with future versions ? (likely yes)
///
/// # Parameters
/// - `external_sender`: Availibility of the external sender extension
///   - Default: None (We don't expose that)
/// - `required_capabilities`: Needed to specify extensions
///   - Option: None (Expose the ability to provide a 3-tuple of Vec<u8>)
///
/// Note, external_sender is an Extension but other config options  might be different

pub fn mls_create_group_config(
    cs: CipherSuite,
    v: ProtocolVersion,
    options: ExtensionList,
) -> Result<GroupConfig, MlsError> {
    Ok(GroupConfig {
        ciphersuite: cs,
        version: v,
        options,
    })
}

/// Client configuration.
///
/// V8  - We could require the library to update the client configuration at runtime
/// Options:
/// - WireFormatPolicy
///     - Default: None (We don't expose that)
/// - padding_size
///     - Default: to some to complete the block to 64 bytes (pick a good value)
/// - max_past_epochs
///     - This is for application messages (cross-epoch)
///     - Option: set the default some small value
/// - number_of_resumption_psks
///     - Default: 0 (We don't expose that)
/// - use_ratchet_tree_extension
///     - Option: default to true
/// - out_of_order_tolerance
///     - This is within an epoch
///     - Option: set the default to small number
/// - maximum_forward_distance
///     - Maximum generation forward within an epoch from the same sender
///     - Default: set to something like 1000 (Signal uses 2000)
/// - Lifetime
///     - Lifetime of a keypackage
///     - This is to indicate the amount of time before which an update needs to happen

pub struct ClientConfig {
    // Add fields as needed
}

// Question: should clients have consistent values for ratchet_tree_exrtensions
pub fn mls_create_client_config(
    max_past_epoch: Option<u32>,
    use_ratchet_tree_extension: bool,
    out_of_order_tolerance: Option<u32>,
    maximum_forward_distance: Option<u32>,
) -> Result<ClientConfig, MlsError> {
    unimplemented!()
}

// pub fn mls_group_propose_update(
//     _pstate: &mut PlatformState,
//     _gid: GroupId,
//     _myself: &Identity,
//     _signature_key: Option<Vec<u8>>,
//     // Below is client config
//     _group_context_extensions: Option<ExtensionList>,
//     _leaf_node_extensions: Option<ExtensionList>,
//     _leaf_node_capabilities: Option<Capabilities>,
//     _lifetime: Option<u64>,
// ) -> Result<MlsMessage, PlatformError> {
//     unimplemented!()
// }

///
/// TODO: Pending commit API
///

// List pending
// Apply pending
// Discard pending
