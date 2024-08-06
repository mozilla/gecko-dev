/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 use api::{BorderRadius, BoxShadowClipMode, ClipMode, ColorF, ColorU, PrimitiveKeyKind, PropertyBinding};
 use api::units::*;
 use crate::clip::{ClipItemKey, ClipItemKeyKind, ClipNodeId};
 use crate::intern::{Handle as InternHandle, InternDebug, Internable};
 use crate::prim_store::{InternablePrimitive, PrimKey, PrimTemplate, PrimTemplateCommonData};
 use crate::prim_store::{PrimitiveInstanceKind, PrimitiveStore};
 use crate::scene_building::{SceneBuilder, IsVisible};
 use crate::spatial_tree::SpatialNodeIndex;
 use crate::gpu_types::BoxShadowStretchMode;
 use crate::render_task_graph::RenderTaskId;
 use crate::internal_types::LayoutPrimitiveInfo;

pub type BoxShadowKey = PrimKey<BoxShadow>;

impl BoxShadowKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        shadow: BoxShadow,
    ) -> Self {
        BoxShadowKey {
            common: info.into(),
            kind: shadow,
        }
    }
}

impl InternDebug for BoxShadowKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, Hash, Eq, PartialEq)]
pub struct BoxShadow {
    pub color: ColorU,
    pub blur_radius: Au,
    pub clip_mode: BoxShadowClipMode,
}

impl IsVisible for BoxShadow {
    fn is_visible(&self) -> bool {
        true
    }
}

pub type BoxShadowDataHandle = InternHandle<BoxShadow>;

impl InternablePrimitive for BoxShadow {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> BoxShadowKey {
        BoxShadowKey::new(info, self)
    }

    fn make_instance_kind(
        _key: BoxShadowKey,
        data_handle: BoxShadowDataHandle,
        _prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::BoxShadow {
            data_handle,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BoxShadowData {
    pub color: ColorF,
    pub blur_radius: f32,
    pub clip_mode: BoxShadowClipMode,
}

impl From<BoxShadow> for BoxShadowData {
    fn from(shadow: BoxShadow) -> Self {
        BoxShadowData {
            color: shadow.color.into(),
            blur_radius: shadow.blur_radius.to_f32_px(),
            clip_mode: shadow.clip_mode,
        }
    }
}

pub type BoxShadowTemplate = PrimTemplate<BoxShadowData>;

impl Internable for BoxShadow {
    type Key = BoxShadowKey;
    type StoreData = BoxShadowTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_BOX_SHADOWS;
}

impl From<BoxShadowKey> for BoxShadowTemplate {
    fn from(shadow: BoxShadowKey) -> Self {
        BoxShadowTemplate {
            common: PrimTemplateCommonData::with_key_common(shadow.common),
            kind: shadow.kind.into(),
        }
    }
}

#[derive(Debug, Clone, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BoxShadowClipSource {
    // Parameters that define the shadow and are constant.
    pub shadow_radius: BorderRadius,
    pub blur_radius: f32,
    pub clip_mode: BoxShadowClipMode,
    pub stretch_mode_x: BoxShadowStretchMode,
    pub stretch_mode_y: BoxShadowStretchMode,

    // The current cache key (in device-pixels), and handles
    // to the cached clip region and blurred texture.
    pub cache_key: Option<(DeviceIntSize, BoxShadowCacheKey)>,
    pub render_task: Option<RenderTaskId>,

    // Local-space size of the required render task size.
    pub shadow_rect_alloc_size: LayoutSize,

    // Local-space size of the required render task size without any downscaling
    // applied. This is needed to stretch the shadow properly.
    pub original_alloc_size: LayoutSize,

    // The minimal shadow rect for the parameters above,
    // used when drawing the shadow rect to be blurred.
    pub minimal_shadow_rect: LayoutRect,

    // Local space rect for the shadow to be drawn or
    // stretched in the shadow primitive.
    pub prim_shadow_rect: LayoutRect,
}

// The blur shader samples BLUR_SAMPLE_SCALE * blur_radius surrounding texels.
pub const BLUR_SAMPLE_SCALE: f32 = 3.0;

// Maximum blur radius for box-shadows (different than blur filters).
// Taken from nsCSSRendering.cpp in Gecko.
pub const MAX_BLUR_RADIUS: f32 = 300.;

// A cache key that uniquely identifies a minimally sized
// and blurred box-shadow rect that can be stored in the
// texture cache and applied to clip-masks.
#[derive(Debug, Clone, Eq, Hash, MallocSizeOf, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BoxShadowCacheKey {
    pub blur_radius_dp: i32,
    pub clip_mode: BoxShadowClipMode,
    // NOTE(emilio): Only the original allocation size needs to be in the cache
    // key, since the actual size is derived from that.
    pub original_alloc_size: DeviceIntSize,
    pub br_top_left: DeviceIntSize,
    pub br_top_right: DeviceIntSize,
    pub br_bottom_right: DeviceIntSize,
    pub br_bottom_left: DeviceIntSize,
    pub device_pixel_scale: Au,
}

impl<'a> SceneBuilder<'a> {
    pub fn add_box_shadow(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_node_id: ClipNodeId,
        prim_info: &LayoutPrimitiveInfo,
        box_offset: &LayoutVector2D,
        color: ColorF,
        mut blur_radius: f32,
        spread_radius: f32,
        border_radius: BorderRadius,
        clip_mode: BoxShadowClipMode,
    ) {
        if color.a == 0.0 {
            return;
        }

        // Inset shadows get smaller as spread radius increases.
        let (spread_amount, prim_clip_mode) = match clip_mode {
            BoxShadowClipMode::Outset => (spread_radius, ClipMode::ClipOut),
            BoxShadowClipMode::Inset => (-spread_radius, ClipMode::Clip),
        };

        // Ensure the blur radius is somewhat sensible.
        blur_radius = f32::min(blur_radius, MAX_BLUR_RADIUS);

        // Adjust the border radius of the box shadow per CSS-spec.
        let shadow_radius = adjust_border_radius_for_box_shadow(border_radius, spread_amount);

        // Apply parameters that affect where the shadow rect
        // exists in the local space of the primitive.
        let shadow_rect = prim_info
            .rect
            .translate(*box_offset)
            .inflate(spread_amount, spread_amount);

        // If blur radius is zero, we can use a fast path with
        // no blur applied.
        if blur_radius == 0.0 {
            // Trivial reject of box-shadows that are not visible.
            if box_offset.x == 0.0 && box_offset.y == 0.0 && spread_amount == 0.0 {
                return;
            }

            let mut clips = Vec::with_capacity(2);
            let (final_prim_rect, clip_radius) = match clip_mode {
                BoxShadowClipMode::Outset => {
                    if shadow_rect.is_empty() {
                        return;
                    }

                    // TODO(gw): Add a fast path for ClipOut + zero border radius!
                    clips.push(ClipItemKey {
                        kind: ClipItemKeyKind::rounded_rect(
                            prim_info.rect,
                            border_radius,
                            ClipMode::ClipOut,
                        ),
                        spatial_node_index,
                    });

                    (shadow_rect, shadow_radius)
                }
                BoxShadowClipMode::Inset => {
                    if !shadow_rect.is_empty() {
                        clips.push(ClipItemKey {
                            kind: ClipItemKeyKind::rounded_rect(
                                shadow_rect,
                                shadow_radius,
                                ClipMode::ClipOut,
                            ),
                            spatial_node_index,
                        });
                    }

                    (prim_info.rect, border_radius)
                }
            };

            clips.push(ClipItemKey {
                kind: ClipItemKeyKind::rounded_rect(
                    final_prim_rect,
                    clip_radius,
                    ClipMode::Clip,
                ),
                spatial_node_index,
            });

            self.add_primitive(
                spatial_node_index,
                clip_node_id,
                &LayoutPrimitiveInfo::with_clip_rect(final_prim_rect, prim_info.clip_rect),
                clips,
                PrimitiveKeyKind::Rectangle {
                    color: PropertyBinding::Value(color.into()),
                },
            );
        } else {
            // Normal path for box-shadows with a valid blur radius.
            let blur_offset = (BLUR_SAMPLE_SCALE * blur_radius).ceil();
            let mut extra_clips = vec![];

            // Add a normal clip mask to clip out the contents
            // of the surrounding primitive.
            extra_clips.push(ClipItemKey {
                kind: ClipItemKeyKind::rounded_rect(
                    prim_info.rect,
                    border_radius,
                    prim_clip_mode,
                ),
                spatial_node_index,
            });

            // Get the local rect of where the shadow will be drawn,
            // expanded to include room for the blurred region.
            let dest_rect = shadow_rect.inflate(blur_offset, blur_offset);

            // Draw the box-shadow as a solid rect, using a box-shadow
            // clip mask item.
            let prim = PrimitiveKeyKind::Rectangle {
                color: PropertyBinding::Value(color.into()),
            };

            // Create the box-shadow clip item.
            let shadow_clip_source = ClipItemKey {
                kind: ClipItemKeyKind::box_shadow(
                    shadow_rect,
                    shadow_radius,
                    dest_rect,
                    blur_radius,
                    clip_mode,
                ),
                spatial_node_index,
            };

            let prim_info = match clip_mode {
                BoxShadowClipMode::Outset => {
                    // Certain spread-radii make the shadow invalid.
                    if shadow_rect.is_empty() {
                        return;
                    }

                    // Add the box-shadow clip source.
                    extra_clips.push(shadow_clip_source);

                    // Outset shadows are expanded by the shadow
                    // region from the original primitive.
                    LayoutPrimitiveInfo::with_clip_rect(dest_rect, prim_info.clip_rect)
                }
                BoxShadowClipMode::Inset => {
                    // If the inner shadow rect contains the prim
                    // rect, no pixels will be shadowed.
                    if border_radius.is_zero() && shadow_rect
                        .inflate(-blur_radius, -blur_radius)
                        .contains_box(&prim_info.rect)
                    {
                        return;
                    }

                    // Inset shadows are still visible, even if the
                    // inset shadow rect becomes invalid (they will
                    // just look like a solid rectangle).
                    if !shadow_rect.is_empty() {
                        extra_clips.push(shadow_clip_source);
                    }

                    // Inset shadows draw inside the original primitive.
                    prim_info.clone()
                }
            };

            self.add_primitive(
                spatial_node_index,
                clip_node_id,
                &prim_info,
                extra_clips,
                prim,
            );
        }
    }
}

fn adjust_border_radius_for_box_shadow(radius: BorderRadius, spread_amount: f32) -> BorderRadius {
    BorderRadius {
        top_left: adjust_corner_for_box_shadow(radius.top_left, spread_amount),
        top_right: adjust_corner_for_box_shadow(radius.top_right, spread_amount),
        bottom_right: adjust_corner_for_box_shadow(radius.bottom_right, spread_amount),
        bottom_left: adjust_corner_for_box_shadow(radius.bottom_left, spread_amount),
    }
}

fn adjust_corner_for_box_shadow(corner: LayoutSize, spread_amount: f32) -> LayoutSize {
    LayoutSize::new(
        adjust_radius_for_box_shadow(corner.width, spread_amount),
        adjust_radius_for_box_shadow(corner.height, spread_amount),
    )
}

fn adjust_radius_for_box_shadow(border_radius: f32, spread_amount: f32) -> f32 {
    if border_radius > 0.0 {
        (border_radius + spread_amount).max(0.0)
    } else {
        0.0
    }
}
