// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::group::{proposal_filter::ProposalBundle, Roster};

#[cfg(feature = "private_message")]
use crate::{
    group::{padding::PaddingMode, Sender},
    WireFormat,
};

use alloc::boxed::Box;
use core::convert::Infallible;
use mls_rs_core::{
    error::IntoAnyError, extension::ExtensionList, group::Member, identity::SigningIdentity,
};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum CommitDirection {
    Send,
    Receive,
}

/// The source of the commit: either a current member or a new member joining
/// via external commit.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum CommitSource {
    ExistingMember(Member),
    NewMember(SigningIdentity),
}

/// Options controlling commit generation
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub struct CommitOptions {
    pub path_required: bool,
    pub ratchet_tree_extension: bool,
    pub single_welcome_message: bool,
    pub allow_external_commit: bool,
}

impl Default for CommitOptions {
    fn default() -> Self {
        CommitOptions {
            path_required: false,
            ratchet_tree_extension: true,
            single_welcome_message: true,
            allow_external_commit: false,
        }
    }
}

impl CommitOptions {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_path_required(self, path_required: bool) -> Self {
        Self {
            path_required,
            ..self
        }
    }

    pub fn with_ratchet_tree_extension(self, ratchet_tree_extension: bool) -> Self {
        Self {
            ratchet_tree_extension,
            ..self
        }
    }

    pub fn with_single_welcome_message(self, single_welcome_message: bool) -> Self {
        Self {
            single_welcome_message,
            ..self
        }
    }

    pub fn with_allow_external_commit(self, allow_external_commit: bool) -> Self {
        Self {
            allow_external_commit,
            ..self
        }
    }
}

/// Options controlling encryption of control and application messages
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
#[non_exhaustive]
pub struct EncryptionOptions {
    #[cfg(feature = "private_message")]
    pub encrypt_control_messages: bool,
    #[cfg(feature = "private_message")]
    pub padding_mode: PaddingMode,
}

#[cfg(feature = "private_message")]
impl EncryptionOptions {
    pub fn new(encrypt_control_messages: bool, padding_mode: PaddingMode) -> Self {
        Self {
            encrypt_control_messages,
            padding_mode,
        }
    }

    pub(crate) fn control_wire_format(&self, sender: Sender) -> WireFormat {
        match sender {
            Sender::Member(_) if self.encrypt_control_messages => WireFormat::PrivateMessage,
            _ => WireFormat::PublicMessage,
        }
    }
}

/// A set of user controlled rules that customize the behavior of MLS.
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
pub trait MlsRules: Send + Sync {
    type Error: IntoAnyError;

    /// This is called when preparing or receiving a commit to pre-process the set of committed
    /// proposals.
    ///
    /// Both proposals received during the current epoch and at the time of commit
    /// will be presented for validation and filtering. Filter and validate will
    /// present a raw list of proposals. Standard MLS rules are applied internally
    /// on the result of these rules.
    ///
    /// Each member of a group MUST apply the same proposal rules in order to
    /// maintain a working group.
    ///
    /// Typically, any invalid proposal should result in an error. The exception are invalid
    /// by-reference proposals processed when _preparing_ a commit, which should be filtered
    /// out instead. This is to avoid the deadlock situation when no commit can be generated
    /// after receiving an invalid set of proposal messages.
    ///
    /// `ProposalBundle` can be arbitrarily modified. For example, a Remove proposal that
    /// removes a moderator can result in adding a GroupContextExtensions proposal that updates
    /// the moderator list in the group context. The resulting `ProposalBundle` is validated
    /// by the library.
    async fn filter_proposals(
        &self,
        direction: CommitDirection,
        source: CommitSource,
        current_roster: &Roster,
        extension_list: &ExtensionList,
        proposals: ProposalBundle,
    ) -> Result<ProposalBundle, Self::Error>;

    /// This is called when preparing a commit to determine various options: whether to enforce an update
    /// path in case it is not mandated by MLS, whether to include the ratchet tree in the welcome
    /// message (if the commit adds members) and whether to generate a single welcome message, or one
    /// welcome message for each added member.
    ///
    /// The `new_roster` and `new_extension_list` describe the group state after the commit.
    fn commit_options(
        &self,
        new_roster: &Roster,
        new_extension_list: &ExtensionList,
        proposals: &ProposalBundle,
    ) -> Result<CommitOptions, Self::Error>;

    /// This is called when sending any packet. For proposals and commits, this determines whether to
    /// encrypt them. For any encrypted packet, this determines the padding mode used.
    ///
    /// Note that for commits, the `current_roster` and `current_extension_list` describe the group state
    /// before the commit, unlike in [commit_options](MlsRules::commit_options).
    fn encryption_options(
        &self,
        current_roster: &Roster,
        current_extension_list: &ExtensionList,
    ) -> Result<EncryptionOptions, Self::Error>;
}

macro_rules! delegate_mls_rules {
    ($implementer:ty) => {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        #[cfg_attr(mls_build_async, maybe_async::must_be_async)]
        impl<T: MlsRules + ?Sized> MlsRules for $implementer {
            type Error = T::Error;

            #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
            async fn filter_proposals(
                &self,
                direction: CommitDirection,
                source: CommitSource,
                current_roster: &Roster,
                extension_list: &ExtensionList,
                proposals: ProposalBundle,
            ) -> Result<ProposalBundle, Self::Error> {
                (**self)
                    .filter_proposals(direction, source, current_roster, extension_list, proposals)
                    .await
            }

            fn commit_options(
                &self,
                roster: &Roster,
                extension_list: &ExtensionList,
                proposals: &ProposalBundle,
            ) -> Result<CommitOptions, Self::Error> {
                (**self).commit_options(roster, extension_list, proposals)
            }

            fn encryption_options(
                &self,
                roster: &Roster,
                extension_list: &ExtensionList,
            ) -> Result<EncryptionOptions, Self::Error> {
                (**self).encryption_options(roster, extension_list)
            }
        }
    };
}

delegate_mls_rules!(Box<T>);
delegate_mls_rules!(&T);

#[derive(Clone, Debug, Default)]
#[non_exhaustive]
/// Default MLS rules with pass-through proposal filter and customizable options.
pub struct DefaultMlsRules {
    pub commit_options: CommitOptions,
    pub encryption_options: EncryptionOptions,
}

impl DefaultMlsRules {
    /// Create new MLS rules with default settings: do not enforce path and do
    /// put the ratchet tree in the extension.
    pub fn new() -> Self {
        Default::default()
    }

    /// Set commit options.
    pub fn with_commit_options(self, commit_options: CommitOptions) -> Self {
        Self {
            commit_options,
            encryption_options: self.encryption_options,
        }
    }

    /// Set encryption options.
    pub fn with_encryption_options(self, encryption_options: EncryptionOptions) -> Self {
        Self {
            commit_options: self.commit_options,
            encryption_options,
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl MlsRules for DefaultMlsRules {
    type Error = Infallible;

    async fn filter_proposals(
        &self,
        _direction: CommitDirection,
        _source: CommitSource,
        _current_roster: &Roster,
        _extension_list: &ExtensionList,
        proposals: ProposalBundle,
    ) -> Result<ProposalBundle, Self::Error> {
        Ok(proposals)
    }

    fn commit_options(
        &self,
        _: &Roster,
        _: &ExtensionList,
        _: &ProposalBundle,
    ) -> Result<CommitOptions, Self::Error> {
        Ok(self.commit_options)
    }

    fn encryption_options(
        &self,
        _: &Roster,
        _: &ExtensionList,
    ) -> Result<EncryptionOptions, Self::Error> {
        Ok(self.encryption_options)
    }
}
