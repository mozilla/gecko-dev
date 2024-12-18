// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::boxed::Box;
use alloc::vec::Vec;

#[cfg(feature = "custom_proposal")]
use itertools::Itertools;

use crate::{
    group::{
        AddProposal, BorrowedProposal, Proposal, ProposalOrRef, ProposalType, ReInitProposal,
        RemoveProposal, Sender,
    },
    ExtensionList,
};

#[cfg(feature = "by_ref_proposal")]
use crate::group::{proposal_cache::CachedProposal, LeafIndex, ProposalRef, UpdateProposal};

#[cfg(feature = "psk")]
use crate::group::PreSharedKeyProposal;

#[cfg(feature = "custom_proposal")]
use crate::group::proposal::CustomProposal;

use crate::group::ExternalInit;

use core::iter::empty;

#[derive(Clone, Debug, Default)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
/// A collection of proposals.
pub struct ProposalBundle {
    pub(crate) additions: Vec<ProposalInfo<AddProposal>>,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) updates: Vec<ProposalInfo<UpdateProposal>>,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) update_senders: Vec<LeafIndex>,
    pub(crate) removals: Vec<ProposalInfo<RemoveProposal>>,
    #[cfg(feature = "psk")]
    pub(crate) psks: Vec<ProposalInfo<PreSharedKeyProposal>>,
    pub(crate) reinitializations: Vec<ProposalInfo<ReInitProposal>>,
    pub(crate) external_initializations: Vec<ProposalInfo<ExternalInit>>,
    pub(crate) group_context_extensions: Vec<ProposalInfo<ExtensionList>>,
    #[cfg(feature = "custom_proposal")]
    pub(crate) custom_proposals: Vec<ProposalInfo<CustomProposal>>,
}

impl ProposalBundle {
    pub fn add(&mut self, proposal: Proposal, sender: Sender, source: ProposalSource) {
        match proposal {
            Proposal::Add(proposal) => self.additions.push(ProposalInfo {
                proposal: *proposal,
                sender,
                source,
            }),
            #[cfg(feature = "by_ref_proposal")]
            Proposal::Update(proposal) => self.updates.push(ProposalInfo {
                proposal,
                sender,
                source,
            }),
            Proposal::Remove(proposal) => self.removals.push(ProposalInfo {
                proposal,
                sender,
                source,
            }),
            #[cfg(feature = "psk")]
            Proposal::Psk(proposal) => self.psks.push(ProposalInfo {
                proposal,
                sender,
                source,
            }),
            Proposal::ReInit(proposal) => self.reinitializations.push(ProposalInfo {
                proposal,
                sender,
                source,
            }),
            Proposal::ExternalInit(proposal) => self.external_initializations.push(ProposalInfo {
                proposal,
                sender,
                source,
            }),
            Proposal::GroupContextExtensions(proposal) => {
                self.group_context_extensions.push(ProposalInfo {
                    proposal,
                    sender,
                    source,
                })
            }
            #[cfg(feature = "custom_proposal")]
            Proposal::Custom(proposal) => self.custom_proposals.push(ProposalInfo {
                proposal,
                sender,
                source,
            }),
        }
    }

    /// Remove the proposal of type `T` at `index`
    ///
    /// Type `T` can be any of the standard MLS proposal types defined in the
    /// [`proposal`](crate::group::proposal) module.
    ///
    /// `index` is consistent with the index returned by any of the proposal
    /// type specific functions in this module.
    pub fn remove<T: Proposable>(&mut self, index: usize) {
        T::remove(self, index);
    }

    /// Iterate over proposals, filtered by type.
    ///
    /// Type `T` can be any of the standard MLS proposal types defined in the
    /// [`proposal`](crate::group::proposal) module.
    pub fn by_type<'a, T: Proposable + 'a>(&'a self) -> impl Iterator<Item = &'a ProposalInfo<T>> {
        T::filter(self).iter()
    }

    /// Retain proposals, filtered by type.
    ///
    /// Type `T` can be any of the standard MLS proposal types defined in the
    /// [`proposal`](crate::group::proposal) module.
    pub fn retain_by_type<T, F, E>(&mut self, mut f: F) -> Result<(), E>
    where
        T: Proposable,
        F: FnMut(&ProposalInfo<T>) -> Result<bool, E>,
    {
        let mut res = Ok(());

        T::retain(self, |p| match f(p) {
            Ok(keep) => keep,
            Err(e) => {
                if res.is_ok() {
                    res = Err(e);
                }
                false
            }
        });

        res
    }

    /// Retain custom proposals in the bundle.
    #[cfg(feature = "custom_proposal")]
    pub fn retain_custom<F, E>(&mut self, mut f: F) -> Result<(), E>
    where
        F: FnMut(&ProposalInfo<CustomProposal>) -> Result<bool, E>,
    {
        let mut res = Ok(());

        self.custom_proposals.retain(|p| match f(p) {
            Ok(keep) => keep,
            Err(e) => {
                if res.is_ok() {
                    res = Err(e);
                }
                false
            }
        });

        res
    }

    /// Retain MLS standard proposals in the bundle.
    pub fn retain<F, E>(&mut self, mut f: F) -> Result<(), E>
    where
        F: FnMut(&ProposalInfo<BorrowedProposal<'_>>) -> Result<bool, E>,
    {
        self.retain_by_type::<AddProposal, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        #[cfg(feature = "by_ref_proposal")]
        self.retain_by_type::<UpdateProposal, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        self.retain_by_type::<RemoveProposal, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        #[cfg(feature = "psk")]
        self.retain_by_type::<PreSharedKeyProposal, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        self.retain_by_type::<ReInitProposal, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        self.retain_by_type::<ExternalInit, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        self.retain_by_type::<ExtensionList, _, _>(|proposal| {
            f(&proposal.as_ref().map(BorrowedProposal::from))
        })?;

        Ok(())
    }

    /// The number of proposals in the bundle
    pub fn length(&self) -> usize {
        let len = 0;

        #[cfg(feature = "psk")]
        let len = len + self.psks.len();

        let len = len + self.external_initializations.len();

        #[cfg(feature = "custom_proposal")]
        let len = len + self.custom_proposals.len();

        #[cfg(feature = "by_ref_proposal")]
        let len = len + self.updates.len();

        len + self.additions.len()
            + self.removals.len()
            + self.reinitializations.len()
            + self.group_context_extensions.len()
    }

    /// Iterate over all proposals inside the bundle.
    pub fn iter_proposals(&self) -> impl Iterator<Item = ProposalInfo<BorrowedProposal<'_>>> {
        let res = self
            .additions
            .iter()
            .map(|p| p.as_ref().map(BorrowedProposal::Add))
            .chain(
                self.removals
                    .iter()
                    .map(|p| p.as_ref().map(BorrowedProposal::Remove)),
            )
            .chain(
                self.reinitializations
                    .iter()
                    .map(|p| p.as_ref().map(BorrowedProposal::ReInit)),
            );

        #[cfg(feature = "by_ref_proposal")]
        let res = res.chain(
            self.updates
                .iter()
                .map(|p| p.as_ref().map(BorrowedProposal::Update)),
        );

        #[cfg(feature = "psk")]
        let res = res.chain(
            self.psks
                .iter()
                .map(|p| p.as_ref().map(BorrowedProposal::Psk)),
        );

        let res = res.chain(
            self.external_initializations
                .iter()
                .map(|p| p.as_ref().map(BorrowedProposal::ExternalInit)),
        );

        let res = res.chain(
            self.group_context_extensions
                .iter()
                .map(|p| p.as_ref().map(BorrowedProposal::GroupContextExtensions)),
        );

        #[cfg(feature = "custom_proposal")]
        let res = res.chain(
            self.custom_proposals
                .iter()
                .map(|p| p.as_ref().map(BorrowedProposal::Custom)),
        );

        res
    }

    /// Iterate over proposal in the bundle, consuming the bundle.
    pub fn into_proposals(self) -> impl Iterator<Item = ProposalInfo<Proposal>> {
        let res = empty();

        #[cfg(feature = "custom_proposal")]
        let res = res.chain(
            self.custom_proposals
                .into_iter()
                .map(|p| p.map(Proposal::Custom)),
        );

        let res = res.chain(
            self.external_initializations
                .into_iter()
                .map(|p| p.map(Proposal::ExternalInit)),
        );

        #[cfg(feature = "psk")]
        let res = res.chain(self.psks.into_iter().map(|p| p.map(Proposal::Psk)));

        #[cfg(feature = "by_ref_proposal")]
        let res = res.chain(self.updates.into_iter().map(|p| p.map(Proposal::Update)));

        res.chain(
            self.additions
                .into_iter()
                .map(|p| p.map(|p| Proposal::Add(alloc::boxed::Box::new(p)))),
        )
        .chain(self.removals.into_iter().map(|p| p.map(Proposal::Remove)))
        .chain(
            self.reinitializations
                .into_iter()
                .map(|p| p.map(Proposal::ReInit)),
        )
        .chain(
            self.group_context_extensions
                .into_iter()
                .map(|p| p.map(Proposal::GroupContextExtensions)),
        )
    }

    pub(crate) fn into_proposals_or_refs(self) -> Vec<ProposalOrRef> {
        self.into_proposals()
            .filter_map(|p| match p.source {
                ProposalSource::ByValue => Some(ProposalOrRef::Proposal(Box::new(p.proposal))),
                #[cfg(feature = "by_ref_proposal")]
                ProposalSource::ByReference(reference) => Some(ProposalOrRef::Reference(reference)),
                _ => None,
            })
            .collect()
    }

    /// Add proposals in the bundle.
    pub fn add_proposals(&self) -> &[ProposalInfo<AddProposal>] {
        &self.additions
    }

    /// Update proposals in the bundle.
    #[cfg(feature = "by_ref_proposal")]
    pub fn update_proposals(&self) -> &[ProposalInfo<UpdateProposal>] {
        &self.updates
    }

    /// Senders of update proposals in the bundle.
    #[cfg(feature = "by_ref_proposal")]
    pub fn update_proposal_senders(&self) -> &[LeafIndex] {
        &self.update_senders
    }

    /// Remove proposals in the bundle.
    pub fn remove_proposals(&self) -> &[ProposalInfo<RemoveProposal>] {
        &self.removals
    }

    /// Pre-shared key proposals in the bundle.
    #[cfg(feature = "psk")]
    pub fn psk_proposals(&self) -> &[ProposalInfo<PreSharedKeyProposal>] {
        &self.psks
    }

    /// Reinit proposals in the bundle.
    pub fn reinit_proposals(&self) -> &[ProposalInfo<ReInitProposal>] {
        &self.reinitializations
    }

    /// External init proposals in the bundle.
    pub fn external_init_proposals(&self) -> &[ProposalInfo<ExternalInit>] {
        &self.external_initializations
    }

    /// Group context extension proposals in the bundle.
    pub fn group_context_ext_proposals(&self) -> &[ProposalInfo<ExtensionList>] {
        &self.group_context_extensions
    }

    /// Custom proposals in the bundle.
    #[cfg(feature = "custom_proposal")]
    pub fn custom_proposals(&self) -> &[ProposalInfo<CustomProposal>] {
        &self.custom_proposals
    }

    pub(crate) fn group_context_extensions_proposal(&self) -> Option<&ProposalInfo<ExtensionList>> {
        self.group_context_extensions.first()
    }

    /// Custom proposal types that are in use within this bundle.
    #[cfg(feature = "custom_proposal")]
    pub fn custom_proposal_types(&self) -> impl Iterator<Item = ProposalType> + '_ {
        #[cfg(feature = "std")]
        let res = self
            .custom_proposals
            .iter()
            .map(|v| v.proposal.proposal_type())
            .unique();

        #[cfg(not(feature = "std"))]
        let res = self
            .custom_proposals
            .iter()
            .map(|v| v.proposal.proposal_type())
            .collect::<alloc::collections::BTreeSet<_>>()
            .into_iter();

        res
    }

    /// Standard proposal types that are in use within this bundle.
    pub fn proposal_types(&self) -> impl Iterator<Item = ProposalType> + '_ {
        let res = (!self.additions.is_empty())
            .then_some(ProposalType::ADD)
            .into_iter()
            .chain((!self.removals.is_empty()).then_some(ProposalType::REMOVE))
            .chain((!self.reinitializations.is_empty()).then_some(ProposalType::RE_INIT));

        #[cfg(feature = "by_ref_proposal")]
        let res = res.chain((!self.updates.is_empty()).then_some(ProposalType::UPDATE));

        #[cfg(feature = "psk")]
        let res = res.chain((!self.psks.is_empty()).then_some(ProposalType::PSK));

        let res = res.chain(
            (!self.external_initializations.is_empty()).then_some(ProposalType::EXTERNAL_INIT),
        );

        #[cfg(not(feature = "custom_proposal"))]
        return res.chain(
            (!self.group_context_extensions.is_empty())
                .then_some(ProposalType::GROUP_CONTEXT_EXTENSIONS),
        );

        #[cfg(feature = "custom_proposal")]
        return res
            .chain(
                (!self.group_context_extensions.is_empty())
                    .then_some(ProposalType::GROUP_CONTEXT_EXTENSIONS),
            )
            .chain(self.custom_proposal_types());
    }
}

impl FromIterator<(Proposal, Sender, ProposalSource)> for ProposalBundle {
    fn from_iter<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = (Proposal, Sender, ProposalSource)>,
    {
        let mut bundle = ProposalBundle::default();
        for (proposal, sender, source) in iter {
            bundle.add(proposal, sender, source);
        }
        bundle
    }
}

#[cfg(feature = "by_ref_proposal")]
impl<'a> FromIterator<(&'a ProposalRef, &'a CachedProposal)> for ProposalBundle {
    fn from_iter<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = (&'a ProposalRef, &'a CachedProposal)>,
    {
        iter.into_iter()
            .map(|(r, p)| {
                (
                    p.proposal.clone(),
                    p.sender,
                    ProposalSource::ByReference(r.clone()),
                )
            })
            .collect()
    }
}

#[cfg(feature = "by_ref_proposal")]
impl<'a> FromIterator<&'a (ProposalRef, CachedProposal)> for ProposalBundle {
    fn from_iter<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = &'a (ProposalRef, CachedProposal)>,
    {
        iter.into_iter().map(|pair| (&pair.0, &pair.1)).collect()
    }
}

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[derive(Clone, Debug, PartialEq)]
pub enum ProposalSource {
    ByValue,
    #[cfg(feature = "by_ref_proposal")]
    ByReference(ProposalRef),
    Local,
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type(opaque))]
#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[non_exhaustive]
/// Proposal description used as input to a
/// [`MlsRules`](crate::MlsRules).
pub struct ProposalInfo<T> {
    /// The underlying proposal value.
    pub proposal: T,
    /// The sender of this proposal.
    pub sender: Sender,
    /// The source of the proposal.
    pub source: ProposalSource,
}

// #[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
impl<T> ProposalInfo<T> {
    /// Create a new ProposalInfo.
    ///
    /// The resulting value will be either transmitted with a commit or
    /// locally injected into a commit resolution depending on the
    /// `can_transmit` flag.
    ///
    /// This function is useful when implementing custom
    /// [`MlsRules`](crate::MlsRules).
    // #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn new(proposal: T, sender: Sender, can_transmit: bool) -> Self {
        let source = if can_transmit {
            ProposalSource::ByValue
        } else {
            ProposalSource::Local
        };

        ProposalInfo {
            proposal,
            sender,
            source,
        }
    }

    // #[cfg(all(feature = "ffi", not(test)))]
    pub fn sender(&self) -> &Sender {
        &self.sender
    }

    // #[cfg(all(feature = "ffi", not(test)))]
    pub fn source(&self) -> &ProposalSource {
        &self.source
    }

    // #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn map<U, F>(self, f: F) -> ProposalInfo<U>
    where
        F: FnOnce(T) -> U,
    {
        ProposalInfo {
            proposal: f(self.proposal),
            sender: self.sender,
            source: self.source,
        }
    }

    // #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn as_ref(&self) -> ProposalInfo<&T> {
        ProposalInfo {
            proposal: &self.proposal,
            sender: self.sender,
            source: self.source.clone(),
        }
    }

    #[inline(always)]
    pub fn is_by_value(&self) -> bool {
        self.source == ProposalSource::ByValue
    }

    #[inline(always)]
    pub fn is_by_reference(&self) -> bool {
        !self.is_by_value()
    }

    /// The [`ProposalRef`] of this proposal if its source is [`ProposalSource::ByReference`]
    #[cfg(feature = "by_ref_proposal")]
    pub fn proposal_ref(&self) -> Option<&ProposalRef> {
        match self.source {
            ProposalSource::ByReference(ref reference) => Some(reference),
            _ => None,
        }
    }
}

// #[cfg(all(feature = "ffi", not(test)))]
// safer_ffi_gen::specialize!(ProposalInfoFfi = ProposalInfo<Proposal>);

pub trait Proposable: Sized {
    const TYPE: ProposalType;

    fn filter(bundle: &ProposalBundle) -> &[ProposalInfo<Self>];
    fn remove(bundle: &mut ProposalBundle, index: usize);
    fn retain<F>(bundle: &mut ProposalBundle, keep: F)
    where
        F: FnMut(&ProposalInfo<Self>) -> bool;
}

macro_rules! impl_proposable {
    ($ty:ty, $proposal_type:ident, $field:ident) => {
        impl Proposable for $ty {
            const TYPE: ProposalType = ProposalType::$proposal_type;

            fn filter(bundle: &ProposalBundle) -> &[ProposalInfo<Self>] {
                &bundle.$field
            }

            fn remove(bundle: &mut ProposalBundle, index: usize) {
                if index < bundle.$field.len() {
                    bundle.$field.remove(index);
                }
            }

            fn retain<F>(bundle: &mut ProposalBundle, keep: F)
            where
                F: FnMut(&ProposalInfo<Self>) -> bool,
            {
                bundle.$field.retain(keep);
            }
        }
    };
}

impl_proposable!(AddProposal, ADD, additions);
#[cfg(feature = "by_ref_proposal")]
impl_proposable!(UpdateProposal, UPDATE, updates);
impl_proposable!(RemoveProposal, REMOVE, removals);
#[cfg(feature = "psk")]
impl_proposable!(PreSharedKeyProposal, PSK, psks);
impl_proposable!(ReInitProposal, RE_INIT, reinitializations);
impl_proposable!(ExternalInit, EXTERNAL_INIT, external_initializations);
impl_proposable!(
    ExtensionList,
    GROUP_CONTEXT_EXTENSIONS,
    group_context_extensions
);
