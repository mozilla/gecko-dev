/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! FFI-friendly mirrors of render pass APIs in [`wgc`].

use crate::id;

/// FFI-friendly analogue of [`std::option::Option`].
///
/// This API defines a representation for values that cannot use niche optimization to have the
/// same layout and ABI as `T`.
///
/// See also: <https://doc.rust-lang.org/nomicon/ffi.html#the-nullable-pointer-optimization>
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FfiOption<T> {
    Some(T),
    None,
}

impl<T> FfiOption<T> {
    pub fn to_std(self) -> std::option::Option<T> {
        match self {
            Self::Some(value) => Some(value),
            Self::None => None,
        }
    }
}

/// FFI-safe analogue of [`wgc::command::RenderPassDepthStencilAttachment`].
#[repr(C)]
#[derive(Clone, Debug, PartialEq)]
pub struct RenderPassDepthStencilAttachment {
    pub view: id::TextureViewId,
    pub depth: PassChannel<FfiOption<f32>>,
    pub stencil: PassChannel<FfiOption<u32>>,
}

impl RenderPassDepthStencilAttachment {
    pub(crate) fn to_wgpu(self) -> wgc::command::RenderPassDepthStencilAttachment {
        let Self {
            view,
            depth,
            stencil,
        } = self;

        wgc::command::RenderPassDepthStencilAttachment {
            view,
            depth: depth.to_wgpu().map_clear_value(|opt| opt.to_std()),
            stencil: stencil.to_wgpu().map_clear_value(|opt| opt.to_std()),
        }
    }
}

/// FFI-safe analogue of [`wgc::command::PassChannel`].
#[repr(C)]
#[derive(Clone, Debug, PartialEq)]
pub struct PassChannel<V> {
    pub load_op: FfiOption<wgc::command::LoadOp<V>>,
    pub store_op: FfiOption<wgc::command::StoreOp>,
    pub read_only: bool,
}

impl<V> PassChannel<V> {
    pub(crate) fn to_wgpu(self) -> wgc::command::PassChannel<V> {
        let Self {
            load_op,
            store_op,
            read_only,
        } = self;

        wgc::command::PassChannel {
            load_op: load_op.to_std(),
            store_op: store_op.to_std(),
            read_only,
        }
    }
}

trait MapClearValue<V1, V2> {
    type Converted;

    fn map_clear_value(self, f: impl FnOnce(V1) -> V2) -> Self::Converted;
}

impl<V1, V2> MapClearValue<V1, V2> for wgc::command::PassChannel<V1> {
    type Converted = wgc::command::PassChannel<V2>;

    fn map_clear_value(self, f: impl FnOnce(V1) -> V2) -> Self::Converted {
        let Self {
            load_op,
            store_op,
            read_only,
        } = self;
        wgc::command::PassChannel {
            load_op: load_op.map(|o| o.map_clear_value(f)),
            store_op,
            read_only,
        }
    }
}

impl<V1, V2> MapClearValue<V1, V2> for wgc::command::LoadOp<V1> {
    type Converted = wgc::command::LoadOp<V2>;

    fn map_clear_value(self, f: impl FnOnce(V1) -> V2) -> Self::Converted {
        match self {
            Self::Clear(value) => wgc::command::LoadOp::Clear(f(value)),
            Self::Load => wgc::command::LoadOp::Load,
        }
    }
}
