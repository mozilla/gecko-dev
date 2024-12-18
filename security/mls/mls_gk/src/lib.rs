/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use mls_platform_api::{
    ClientIdentifiers, ExporterOutput, GroupIdEpoch, GroupMembers, MlsCommitOutput, Received,
};
use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::nsACString;
use thin_vec::ThinVec;

mod storage;
use storage::get_key_path;
use storage::get_storage_key;
use storage::get_storage_path;

// Access the platform state for the given storage prefix
pub fn state_access(
    storage_prefix: &nsACString,
) -> Result<mls_platform_api::PlatformState, nsresult> {
    if storage_prefix.is_empty() {
        log::error!("Input storage prefix cannot be empty");
        return Err(NS_ERROR_INVALID_ARG);
    };

    let db_path = get_storage_path(storage_prefix);
    let Ok(db_key) = get_storage_key(storage_prefix) else {
        log::error!("Failed to get storage key");
        return Err(NS_ERROR_FAILURE);
    };

    mls_platform_api::state_access(&db_path, &db_key).map_err(|e| {
        log::error!("Failed to access state: {:?}", e);
        NS_ERROR_FAILURE
    })
}

#[no_mangle]
pub extern "C" fn mls_state_delete(storage_prefix: &nsACString) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_state_delete");

    // Retrieve the database path from the caller and retrieve the key
    let db_path = get_storage_path(storage_prefix);
    let key_path = get_key_path(storage_prefix);

    // Delete the key file which is owned by the storage module
    let key_removal_result = match std::fs::remove_file(&key_path) {
        Ok(_) => Ok(()),
        Err(e) => {
            log::error!("{:?}", e);
            Err(NS_ERROR_FAILURE)
        }
    };

    // Delete the database file which is owned by the mls-platform-api library
    // Note: we do not check if the key is valid before destructing the database
    let db_removal_result = match mls_platform_api::state_delete(&db_path) {
        Ok(_) => Ok(()),
        Err(e) => {
            log::error!("{:?}", e);
            Err(NS_ERROR_FAILURE)
        }
    };

    // Return an error if either operation failed
    if key_removal_result.is_err() || db_removal_result.is_err() {
        return NS_ERROR_FAILURE;
    }

    // Log the name of the database and keys that were deleted
    log::debug!(" (input) storage_prefix: {:?}", storage_prefix);
    log::info!("State deleted successfully");
    NS_OK
}

#[repr(C)]
pub struct GkGroupIdEpoch {
    pub group_id: ThinVec<u8>,
    pub group_epoch: ThinVec<u8>,
}

impl From<GroupIdEpoch> for GkGroupIdEpoch {
    fn from(v: GroupIdEpoch) -> Self {
        let GroupIdEpoch {
            group_id,
            group_epoch,
        } = v;
        Self {
            group_id: group_id.into(),
            group_epoch: ThinVec::from(group_epoch.to_le_bytes()),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn mls_state_delete_group(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    ret_group_id_epoch: &mut GkGroupIdEpoch,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_state_delete_group");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Call the platform API to delete the group for the selected client
    let groupid_and_epoch =
        match mls_platform_api::state_delete_group(&mut pstate, group_id_bytes, identifier_bytes) {
            Ok(gid) => gid,
            Err(e) => {
                log::error!("{:?}", e);
                return NS_ERROR_FAILURE;
            }
        };

    log::debug!(
        " (returns) Group Identifier: {:?}",
        hex::encode(&groupid_and_epoch.group_id)
    );

    log::debug!(
        " (returns) Group Epoch: {:?}",
        hex::encode(&groupid_and_epoch.group_epoch.to_le_bytes())
    );

    // Write the results
    *ret_group_id_epoch = groupid_and_epoch.into();
    log::info!("Successfully deleted group");

    NS_OK
}

#[no_mangle]
pub extern "C" fn mls_generate_signature_keypair(
    storage_prefix: &nsACString,
    ret_identifier: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_generate_signature_keypair");

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Note: We set the GroupConfig to default for now
    let default_gc = mls_platform_api::GroupConfig::default();

    // Generate the signature keypair
    let identifier =
        match mls_platform_api::mls_generate_signature_keypair(&mut pstate, default_gc.ciphersuite)
        {
            Ok(id) => id,
            Err(e) => {
                log::error!("{:?}", e);
                return NS_ERROR_FAILURE;
            }
        };

    // Log the identifier
    log::debug!(
        " (returns) Client Identifier: {:?}",
        hex::encode(&identifier)
    );

    // Write the result to ret_val
    ret_identifier.extend_from_slice(&identifier);

    log::info!("Successfully generated signature keypair");

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_generate_credential_basic(
    cred_content_bytes_ptr: *const u8,
    cred_content_bytes_len: usize,
    ret_credential: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_generate_credential_basic");

    // Validate the inputs
    if cred_content_bytes_len == 0 {
        log::error!("Credential content argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let cred_content_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(cred_content_bytes_ptr, cred_content_bytes_len) };

    // Generate the basic credential
    let credential_bytes = match mls_platform_api::mls_generate_credential_basic(cred_content_bytes)
    {
        Ok(cred) => cred,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the credential
    log::debug!(
        " (returns) Credential: {:?}",
        hex::encode(&credential_bytes)
    );

    // Write the result to ret_val
    ret_credential.extend_from_slice(&credential_bytes);

    log::info!("Successfully generated basic credential");
    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_generate_keypackage(
    storage_prefix: &nsACString,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    credential_bytes_ptr: *const u8,
    credential_bytes_len: usize,
    ret_keypackage: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_generate_keypackage");

    // Validate the inputs
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if credential_bytes_len == 0 {
        log::error!("Credential argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let credential_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(credential_bytes_ptr, credential_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Generate the key package
    let key_package = match mls_platform_api::mls_generate_key_package(
        &pstate,
        identifier_bytes,
        credential_bytes,
        &Default::default(),
    ) {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    let key_package_bytes = match key_package.to_bytes() {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Write the result
    ret_keypackage.extend_from_slice(&key_package_bytes);

    log::debug!(
        " (returns) Key Package: {:?}",
        hex::encode(&key_package_bytes)
    );

    log::info!("Successfully generated key package");

    NS_OK
}

#[repr(C)]
pub struct GkClientIdentifiers {
    pub identity: ThinVec<u8>,
    pub credential: ThinVec<u8>,
}

impl From<ClientIdentifiers> for GkClientIdentifiers {
    fn from(v: ClientIdentifiers) -> Self {
        let ClientIdentifiers {
            identity,
            credential,
        } = v;
        Self {
            identity: identity.into(),
            credential: credential.into(),
        }
    }
}

#[repr(C)]
pub struct GkGroupMembers {
    pub group_id: ThinVec<u8>,
    pub group_epoch: ThinVec<u8>,
    pub group_members: ThinVec<GkClientIdentifiers>,
}

impl From<GroupMembers> for GkGroupMembers {
    fn from(v: GroupMembers) -> Self {
        let GroupMembers {
            group_id,
            group_epoch,
            group_members,
        } = v;
        Self {
            group_id: group_id.into(),
            group_epoch: ThinVec::from(group_epoch.to_le_bytes()),
            group_members: group_members.into_iter().map(Into::into).collect(),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_members(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    ret_group_members: &mut GkGroupMembers,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_members");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the group membership
    let Ok(group_members) =
        mls_platform_api::mls_group_members(&mut pstate, group_id_bytes, identifier_bytes)
    else {
        log::error!("Failed to retrieve group members");
        return NS_ERROR_FAILURE;
    };

    // Log the result
    log::debug!(" (returns) Group Members: {:?}", group_members);

    // Write the result
    *ret_group_members = group_members.into();

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_create(
    storage_prefix: &nsACString,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    credential_bytes_ptr: *const u8,
    credential_bytes_len: usize,
    opt_group_id_bytes_ptr: *const u8,
    opt_group_id_bytes_len: usize,
    ret_group_id_epoch: &mut GkGroupIdEpoch,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_create");

    // Validate the inputs
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if credential_bytes_len == 0 {
        log::error!("Credential argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let credential_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(credential_bytes_ptr, credential_bytes_len) };
    let opt_group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(opt_group_id_bytes_ptr, opt_group_id_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Note: we implicitely require a 32 bytes identifier
    let gid_opt_vec: Vec<u8> = opt_group_id_bytes.to_vec();
    let gid_opt_res = if opt_group_id_bytes_len != 32 {
        log::debug!(
            "Optional group identifier provided has incorrect length, generating a random GID..."
        );
        None
    } else {
        Some(gid_opt_vec.as_ref())
    };

    // Retrieve the group membership
    let gide = match mls_platform_api::mls_group_create(
        &mut pstate,
        identifier_bytes,
        credential_bytes,
        gid_opt_res,
        None,
        &Default::default(),
    ) {
        Ok(v) => v,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(
        " (returns) Group Identifier created: {:?}",
        hex::encode(&gide.group_id)
    );

    // Write the result to ret_val
    *ret_group_id_epoch = gide.into();

    log::info!("Successfully created group");
    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_add(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    keypackage_bytes_ptr: *const u8,
    keypackage_bytes_len: usize,
    ret_commit_output: &mut GkMlsCommitOutput,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_add");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if keypackage_bytes_len == 0 {
        log::error!("Key Package argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let keypackage_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(keypackage_bytes_ptr, keypackage_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the key_package from the caller
    let key_package = match mls_platform_api::MlsMessage::from_bytes(&keypackage_bytes) {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_INVALID_ARG;
        }
    };

    let Ok(commit_output) = mls_platform_api::mls_group_add(
        &mut pstate,
        group_id_bytes,
        identifier_bytes,
        vec![key_package],
    ) else {
        log::error!("Failed to add client to the group");
        return NS_ERROR_FAILURE;
    };

    // Log the result
    log::debug!(" (returns) Commit: {:?}", &commit_output.commit);
    log::debug!(" (returns) Welcome: {:?}", &commit_output.welcome);
    log::debug!(" (returns) Identity: {:?}", &commit_output.identity);

    // Write the result
    *ret_commit_output = commit_output.into();

    log::info!("Successfully added client to the group");

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_propose_add(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    keypackage_bytes_ptr: *const u8,
    keypackage_bytes_len: usize,
    ret_proposal: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_propose_add");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if keypackage_bytes_len == 0 {
        log::error!("Key Package argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let keypackage_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(keypackage_bytes_ptr, keypackage_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the key_package from the caller
    let key_package = match mls_platform_api::MlsMessage::from_bytes(&keypackage_bytes) {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_INVALID_ARG;
        }
    };

    // Retrieve the group membership
    let proposal = match mls_platform_api::mls_group_propose_add(
        &mut pstate,
        group_id_bytes,
        identifier_bytes,
        key_package,
    ) {
        Ok(m) => m,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(
        " (returns) Add Proposal: {:?}",
        hex::encode(&proposal.to_bytes().unwrap())
    );

    // Write the result to ret_val
    ret_proposal.extend_from_slice(&proposal.to_bytes().unwrap());

    log::info!("Successfully proposed adding client to the group");
    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_remove(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    rem_identifier_bytes_ptr: *const u8,
    rem_identifier_bytes_len: usize,
    ret_commit_output: &mut GkMlsCommitOutput,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_remove");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if rem_identifier_bytes_len == 0 {
        log::error!("Identifier to remove argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let rem_identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(rem_identifier_bytes_ptr, rem_identifier_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the group membership
    let commit_output = match mls_platform_api::mls_group_remove(
        &mut pstate,
        group_id_bytes,
        identifier_bytes,
        rem_identifier_bytes,
    ) {
        Ok(gid) => gid,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(" (returns) Commit: {:?}", &commit_output.commit);
    log::debug!(" (returns) Welcome: {:?}", &commit_output.welcome);
    log::debug!(" (returns) Identity: {:?}", &commit_output.identity);

    // Write the result
    *ret_commit_output = commit_output.into();

    log::info!("Successfully removed client from the group");

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_propose_remove(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    rem_identifier_bytes_ptr: *const u8,
    rem_identifier_bytes_len: usize,
    ret_proposal: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::info!("Entering mls_group_propose_remove");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if rem_identifier_bytes_len == 0 {
        log::error!("Identifier to remove argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let rem_identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(rem_identifier_bytes_ptr, rem_identifier_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the group membership
    let proposal = match mls_platform_api::mls_group_propose_remove(
        &mut pstate,
        group_id_bytes,
        identifier_bytes,
        rem_identifier_bytes,
    ) {
        Ok(gid) => gid,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    let proposal_bytes = match proposal.to_bytes() {
        Ok(gid) => gid,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::info!(
        " (returns) Remove Proposal: {:?}",
        hex::encode(&proposal_bytes)
    );

    // Write the result
    ret_proposal.extend_from_slice(&proposal_bytes);

    log::info!("Successfully proposed removing client from the group");

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_join(
    storage_prefix: &nsACString,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    welcome_bytes_ptr: *const u8,
    welcome_bytes_len: usize,
    ret_group_id_epoch: &mut GkGroupIdEpoch,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_join");

    // Validate the inputs
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if welcome_bytes_len == 0 {
        log::error!("Welcome message argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let welcome_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(welcome_bytes_ptr, welcome_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the welcome message from the caller
    let welcome = match mls_platform_api::MlsMessage::from_bytes(&welcome_bytes) {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_INVALID_ARG;
        }
    };

    // Retrieve the group membership
    let gide = match mls_platform_api::mls_group_join(&pstate, identifier_bytes, &welcome, None) {
        Ok(gid) => gid,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(
        " (returns) Group Identifier joined: {:?}",
        hex::encode(&gide.group_id)
    );

    // Write the result to ret_val
    *ret_group_id_epoch = gide.into();

    log::info!("Successfully joined group");
    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_group_close(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    ret_commit_output: &mut GkMlsCommitOutput,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_group_close");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(mut pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the commit output
    let commit_output =
        match mls_platform_api::mls_group_close(&mut pstate, group_id_bytes, identifier_bytes) {
            Ok(gid) => gid,
            Err(e) => {
                log::error!("{:?}", e);
                return NS_ERROR_FAILURE;
            }
        };

    // Log the result
    log::debug!(" (returns) Commit: {:?}", &commit_output.commit);
    log::debug!(" (returns) Welcome: {:?}", &commit_output.welcome);
    log::debug!(" (returns) Identity: {:?}", &commit_output.identity);

    // Write the result
    *ret_commit_output = commit_output.into();

    log::info!("Successfully closed group");

    NS_OK
}

#[repr(C)]
pub struct GkMlsCommitOutput {
    pub commit: ThinVec<u8>,
    pub welcome: ThinVec<u8>,
    pub group_info: ThinVec<u8>,
    pub ratchet_tree: ThinVec<u8>,
    pub identity: ThinVec<u8>,
}

impl From<MlsCommitOutput> for GkMlsCommitOutput {
    fn from(v: MlsCommitOutput) -> Self {
        let MlsCommitOutput {
            commit,
            welcome,
            group_info,
            ratchet_tree,
            identity,
        } = v;
        Self {
            commit: commit.to_bytes().unwrap_or_default().into(),
            welcome: welcome
                .first()
                .and_then(|f| f.to_bytes().ok())
                .map_or(ThinVec::new(), |b| b.into()),
            group_info: group_info
                .and_then(|gi| gi.to_bytes().ok())
                .map_or(ThinVec::new(), |b| b.into()),
            ratchet_tree: ratchet_tree.unwrap_or_default().into(),
            identity: identity.unwrap_or_default().into(),
        }
    }
}

#[repr(C)]
/// cbindgen:derive-constructor=false
/// cbindgen:derive-tagged-enum-copy-constructor=false
/// cbindgen:derive-tagged-enum-copy-assignment=false
pub enum GkReceived {
    None,
    ApplicationMessage(ThinVec<u8>),
    GroupIdEpoch(GkGroupIdEpoch),
    CommitOutput(GkMlsCommitOutput),
}

#[no_mangle]
pub unsafe extern "C" fn mls_receive(
    storage_prefix: &nsACString,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    message_bytes_ptr: *const u8,
    message_bytes_len: usize,
    ret_group_id: &mut ThinVec<u8>,
    ret_received: &mut GkReceived,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_receive");

    // Validate the inputs
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if message_bytes_len == 0 {
        log::error!("Message argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let message_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(message_bytes_ptr, message_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the message from the caller
    let message = match mls_platform_api::MlsMessage::from_bytes(&message_bytes) {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_INVALID_ARG;
        }
    };

    // Retrieve the received output and the group identifier
    let (gid, received) = match mls_platform_api::mls_receive(
        &pstate,
        identifier_bytes,
        &mls_platform_api::MessageOrAck::MlsMessage(message),
    ) {
        Ok(recv) => recv,
        Err(e) => {
            log::error!("Failed to receive message: {:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(" (returns) Group Identifier: {:?}", hex::encode(&gid));

    // Write the group id to ret_group_id
    ret_group_id.extend_from_slice(&gid);
    *ret_received = match received {
        Received::ApplicationMessage(message) => {
            log::info!("Received an ApplicationMessage");
            log::debug!(
                " (returns) Received Application Message: {:?}",
                hex::encode(&message)
            );
            GkReceived::ApplicationMessage(message.into())
        }
        Received::GroupIdEpoch(epoch) => {
            log::info!("Received a GroupIdEpoch");
            log::debug!(" (returns) Received GroupIdEpoch: {:?}", epoch);
            GkReceived::GroupIdEpoch(epoch.into())
        }
        Received::CommitOutput(commit_output) => {
            log::info!("Received a CommitOutput");
            log::debug!(" (returns) Received CommitOutput: {:?}", commit_output);
            GkReceived::CommitOutput(commit_output.into())
        }
        Received::None => {
            log::info!("Received None");
            GkReceived::None
        }
    };

    log::info!("Successfully received message");

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_receive_ack(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    ret_received: &mut GkReceived,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_receive");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the received output and the group identifier
    let (gid, received) = match mls_platform_api::mls_receive(
        &pstate,
        identifier_bytes,
        &mls_platform_api::MessageOrAck::Ack(group_id_bytes.to_vec()),
    ) {
        Ok(recv) => recv,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(" (returns) Group Identifier: {:?}", hex::encode(&gid));

    // Write the group id to ret_group_id
    *ret_received = match received {
        Received::GroupIdEpoch(epoch) => {
            log::info!("Received a GroupIdEpoch");
            log::debug!(" (returns) Received GroupIdEpoch: {:?}", epoch);
            GkReceived::GroupIdEpoch(epoch.into())
        }
        _ => {
            log::info!("Unexpected received type for mls_receive_ack");
            GkReceived::None
        }
    };

    log::info!("Successfully received ack message");

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_send(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    message_bytes_ptr: *const u8,
    message_bytes_len: usize,
    ret_encrypted: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_send");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    // Note: We allow empty messages as they could be used as control

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let message_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(message_bytes_ptr, message_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the ciphertext
    let ciphertext = match mls_platform_api::mls_send(
        &pstate,
        group_id_bytes,
        identifier_bytes,
        message_bytes,
    ) {
        Ok(ctx) => ctx,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Retrieve the message from the caller
    let ciphertext_bytes = match mls_platform_api::MlsMessage::to_bytes(&ciphertext) {
        Ok(ctx) => ctx,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Log the result
    log::debug!(" (input) Message: {:?}", hex::encode(&message_bytes));
    log::debug!(
        " (returns) Ciphertext: {:?}",
        hex::encode(&ciphertext_bytes)
    );

    // Write the result to ret_val
    ret_encrypted.extend_from_slice(&ciphertext_bytes);

    log::info!("Successfully encrypted message");
    NS_OK
}

#[repr(C)]
pub struct GkExporterOutput {
    pub group_id: ThinVec<u8>,
    pub group_epoch: ThinVec<u8>,
    pub label: ThinVec<u8>,
    pub context: ThinVec<u8>,
    pub exporter: ThinVec<u8>,
}

impl From<ExporterOutput> for GkExporterOutput {
    fn from(v: ExporterOutput) -> Self {
        let ExporterOutput {
            group_id,
            group_epoch,
            label,
            context,
            exporter,
        } = v;
        Self {
            group_id: group_id.into(),
            group_epoch: ThinVec::from(group_epoch.to_le_bytes()),
            label: label.into(),
            context: context.into(),
            exporter: exporter.into(),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn mls_derive_exporter(
    storage_prefix: &nsACString,
    group_id_bytes_ptr: *const u8,
    group_id_bytes_len: usize,
    identifier_bytes_ptr: *const u8,
    identifier_bytes_len: usize,
    label_bytes_ptr: *const u8,
    label_bytes_len: usize,
    context_bytes_ptr: *const u8,
    context_bytes_len: usize,
    len: u64,
    ret_exporter_output: &mut GkExporterOutput,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_derive_exporter");

    // Validate the inputs
    if group_id_bytes_len == 0 {
        log::error!("Group Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if identifier_bytes_len == 0 {
        log::error!("Identifier argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if label_bytes_len == 0 {
        log::error!("Label argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }
    if len == 0 {
        log::error!("Length argument cannot be zero");
        return NS_ERROR_INVALID_ARG;
    }

    // Convert the raw pointers to slices
    let group_id_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(group_id_bytes_ptr, group_id_bytes_len) };
    let identifier_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(identifier_bytes_ptr, identifier_bytes_len) };
    let label_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(label_bytes_ptr, label_bytes_len) };
    let context_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(context_bytes_ptr, context_bytes_len) };

    // Retrieve the platform state based on the storage prefix
    let Ok(pstate) = state_access(storage_prefix) else {
        return NS_ERROR_FAILURE;
    };

    // Retrieve the exporter output
    let exporter_output = match mls_platform_api::mls_derive_exporter(
        &pstate,
        group_id_bytes,
        identifier_bytes,
        label_bytes,
        context_bytes,
        len,
    ) {
        Ok(exp) => exp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    log::debug!(
        " (returns) Exporter: {:?}",
        hex::encode(&exporter_output.exporter)
    );

    // Handle group identifier
    *ret_exporter_output = exporter_output.into();

    log::info!("Successfully derived exporter");
    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mls_get_group_id(
    message_bytes_ptr: *const u8,
    message_bytes_len: usize,
    ret_group_id: &mut ThinVec<u8>,
) -> nsresult {
    // Log the function call
    log::debug!("Entering mls_get_group_id");

    // Validate the inputs
    if message_bytes_len == 0 {
        log::error!("Message argument cannot be empty");
        return NS_ERROR_INVALID_ARG;
    }

    let message_bytes: &[u8] =
        unsafe { std::slice::from_raw_parts(message_bytes_ptr, message_bytes_len) };

    // Retrieve the message from the caller
    let message = match mls_platform_api::MlsMessage::from_bytes(&message_bytes) {
        Ok(kp) => kp,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_INVALID_ARG;
        }
    };

    // Retrieve the group identifier
    let gid = match mls_platform_api::mls_get_group_id(&mls_platform_api::MessageOrAck::MlsMessage(
        message,
    )) {
        Ok(recv) => recv,
        Err(e) => {
            log::error!("{:?}", e);
            return NS_ERROR_FAILURE;
        }
    };

    // Write the group id to ret_group_id
    ret_group_id.extend_from_slice(&gid);

    // Log the result
    log::debug!(" (returns) Group Identifier: {:?}", hex::encode(&gid));
    log::info!("Successfully retrieved group id");

    NS_OK
}
