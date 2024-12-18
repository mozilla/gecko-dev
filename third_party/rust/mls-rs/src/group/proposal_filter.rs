// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

mod bundle;
mod filtering_common;

#[cfg(feature = "by_ref_proposal")]
mod filtering;
#[cfg(not(feature = "by_ref_proposal"))]
pub mod filtering_lite;
#[cfg(all(feature = "custom_proposal", not(feature = "by_ref_proposal")))]
use filtering_lite as filtering;

pub use bundle::{ProposalBundle, ProposalInfo, ProposalSource};

#[cfg(feature = "by_ref_proposal")]
pub(crate) use filtering::FilterStrategy;

pub(crate) use filtering_common::ProposalApplier;

#[cfg(all(feature = "by_ref_proposal", test))]
pub(crate) use filtering::proposer_can_propose;
