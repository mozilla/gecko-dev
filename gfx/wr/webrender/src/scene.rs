/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BuiltDisplayList, ColorF, DynamicProperties, Epoch, LayoutSize};
use api::{FilterOp, LayoutTransform, PipelineId, PropertyBinding, PropertyBindingId};
use api::{ItemRange, MixBlendMode, StackingContext};
use internal_types::FastHashMap;
use std::sync::Arc;

/// Stores a map of the animated property bindings for the current display list. These
/// can be used to animate the transform and/or opacity of a display list without
/// re-submitting the display list itself.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SceneProperties {
    transform_properties: FastHashMap<PropertyBindingId, LayoutTransform>,
    float_properties: FastHashMap<PropertyBindingId, f32>,
    current_properties: DynamicProperties,
    pending_properties: Option<DynamicProperties>,
}

impl SceneProperties {
    pub fn new() -> Self {
        SceneProperties {
            transform_properties: FastHashMap::default(),
            float_properties: FastHashMap::default(),
            current_properties: DynamicProperties::default(),
            pending_properties: None,
        }
    }

    /// Set the current property list for this display list.
    pub fn set_properties(&mut self, properties: DynamicProperties) {
        self.pending_properties = Some(properties);
    }

    /// Add to the current property list for this display list.
    pub fn add_properties(&mut self, properties: DynamicProperties) {
        let mut pending_properties = self.pending_properties
            .take()
            .unwrap_or_default();

        pending_properties.transforms.extend(properties.transforms);
        pending_properties.floats.extend(properties.floats);

        self.pending_properties = Some(pending_properties);
    }

    /// Flush any pending updates to the scene properties. Returns
    /// true if the properties have changed since the last flush
    /// was called. This code allows properties to be changed by
    /// multiple set_properties and add_properties calls during a
    /// single transaction, and still correctly determine if any
    /// properties have changed. This can have significant power
    /// saving implications, allowing a frame build to be skipped
    /// if the properties haven't changed in many cases.
    pub fn flush_pending_updates(&mut self) -> bool {
        let mut properties_changed = false;

        if let Some(ref pending_properties) = self.pending_properties {
            if *pending_properties != self.current_properties {
                self.transform_properties.clear();
                self.float_properties.clear();

                for property in &pending_properties.transforms {
                    self.transform_properties
                        .insert(property.key.id, property.value);
                }

                for property in &pending_properties.floats {
                    self.float_properties
                        .insert(property.key.id, property.value);
                }

                self.current_properties = pending_properties.clone();
                properties_changed = true;
            }
        }

        properties_changed
    }

    /// Get the current value for a transform property.
    pub fn resolve_layout_transform(
        &self,
        property: &PropertyBinding<LayoutTransform>,
    ) -> LayoutTransform {
        match *property {
            PropertyBinding::Value(value) => value,
            PropertyBinding::Binding(ref key, v) => {
                self.transform_properties
                    .get(&key.id)
                    .cloned()
                    .unwrap_or(v)
            }
        }
    }

    /// Get the current value for a float property.
    pub fn resolve_float(
        &self,
        property: &PropertyBinding<f32>
    ) -> f32 {
        match *property {
            PropertyBinding::Value(value) => value,
            PropertyBinding::Binding(ref key, v) => {
                self.float_properties
                    .get(&key.id)
                    .cloned()
                    .unwrap_or(v)
            }
        }
    }

    pub fn float_properties(&self) -> &FastHashMap<PropertyBindingId, f32> {
        &self.float_properties
    }
}

/// A representation of the layout within the display port for a given document or iframe.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone)]
pub struct ScenePipeline {
    pub pipeline_id: PipelineId,
    pub viewport_size: LayoutSize,
    pub content_size: LayoutSize,
    pub background_color: Option<ColorF>,
    pub display_list: BuiltDisplayList,
}

/// A complete representation of the layout bundling visible pipelines together.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone)]
pub struct Scene {
    pub root_pipeline_id: Option<PipelineId>,
    pub pipelines: FastHashMap<PipelineId, Arc<ScenePipeline>>,
    pub pipeline_epochs: FastHashMap<PipelineId, Epoch>,
}

impl Scene {
    pub fn new() -> Self {
        Scene {
            root_pipeline_id: None,
            pipelines: FastHashMap::default(),
            pipeline_epochs: FastHashMap::default(),
        }
    }

    pub fn set_root_pipeline_id(&mut self, pipeline_id: PipelineId) {
        self.root_pipeline_id = Some(pipeline_id);
    }

    pub fn get_display_list_for_pipeline(&self, pipeline_id: PipelineId) -> &BuiltDisplayList {
        &self.pipelines
            .get(&pipeline_id)
            .expect("Expected to find display list for pipeline")
            .display_list
    }

    pub fn set_display_list(
        &mut self,
        pipeline_id: PipelineId,
        epoch: Epoch,
        display_list: BuiltDisplayList,
        background_color: Option<ColorF>,
        viewport_size: LayoutSize,
        content_size: LayoutSize,
    ) {
        let new_pipeline = ScenePipeline {
            pipeline_id,
            viewport_size,
            content_size,
            background_color,
            display_list,
        };

        self.pipelines.insert(pipeline_id, Arc::new(new_pipeline));
        self.pipeline_epochs.insert(pipeline_id, epoch);
    }

    pub fn remove_pipeline(&mut self, pipeline_id: PipelineId) {
        if self.root_pipeline_id == Some(pipeline_id) {
            self.root_pipeline_id = None;
        }
        self.pipelines.remove(&pipeline_id);
        self.pipeline_epochs.remove(&pipeline_id);
    }

    pub fn update_epoch(&mut self, pipeline_id: PipelineId, epoch: Epoch) {
        self.pipeline_epochs.insert(pipeline_id, epoch);
    }

    pub fn has_root_pipeline(&self) -> bool {
        if let Some(ref root_id) = self.root_pipeline_id {
            return self.pipelines.contains_key(root_id);
        }

        false
    }
}

/// An arbitrary number which we assume opacity is invisible below.
pub const OPACITY_EPSILON: f32 = 0.001;

pub trait FilterOpHelpers {
    fn is_visible(&self) -> bool;
    fn is_noop(&self) -> bool;
}

impl FilterOpHelpers for FilterOp {
    fn is_visible(&self) -> bool {
        match *self {
            FilterOp::Identity |
            FilterOp::Blur(..) |
            FilterOp::Brightness(..) |
            FilterOp::Contrast(..) |
            FilterOp::Grayscale(..) |
            FilterOp::HueRotate(..) |
            FilterOp::Invert(..) |
            FilterOp::Saturate(..) |
            FilterOp::Sepia(..) |
            FilterOp::DropShadow(..) |
            FilterOp::ColorMatrix(..) |
            FilterOp::SrgbToLinear |
            FilterOp::LinearToSrgb  => true,
            FilterOp::Opacity(_, amount) => {
                amount > OPACITY_EPSILON
            }
        }
    }

    fn is_noop(&self) -> bool {
        match *self {
            FilterOp::Identity => false, // this is intentional
            FilterOp::Blur(length) => length == 0.0,
            FilterOp::Brightness(amount) => amount == 1.0,
            FilterOp::Contrast(amount) => amount == 1.0,
            FilterOp::Grayscale(amount) => amount == 0.0,
            FilterOp::HueRotate(amount) => amount == 0.0,
            FilterOp::Invert(amount) => amount == 0.0,
            FilterOp::Opacity(_, amount) => amount >= 1.0,
            FilterOp::Saturate(amount) => amount == 1.0,
            FilterOp::Sepia(amount) => amount == 0.0,
            FilterOp::DropShadow(offset, blur, _) => {
                offset.x == 0.0 && offset.y == 0.0 && blur == 0.0
            },
            FilterOp::ColorMatrix(matrix) => {
                matrix == [1.0, 0.0, 0.0, 0.0,
                           0.0, 1.0, 0.0, 0.0,
                           0.0, 0.0, 1.0, 0.0,
                           0.0, 0.0, 0.0, 1.0,
                           0.0, 0.0, 0.0, 0.0]
            }
            FilterOp::SrgbToLinear | FilterOp::LinearToSrgb => false,
        }
    }
}

pub trait StackingContextHelpers {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode>;
    fn filter_ops_for_compositing(
        &self,
        display_list: &BuiltDisplayList,
        input_filters: ItemRange<FilterOp>,
    ) -> Vec<FilterOp>;
}

impl StackingContextHelpers for StackingContext {
    fn mix_blend_mode_for_compositing(&self) -> Option<MixBlendMode> {
        match self.mix_blend_mode {
            MixBlendMode::Normal => None,
            _ => Some(self.mix_blend_mode),
        }
    }

    fn filter_ops_for_compositing(
        &self,
        display_list: &BuiltDisplayList,
        input_filters: ItemRange<FilterOp>,
    ) -> Vec<FilterOp> {
        // TODO(gw): Now that we resolve these later on,
        //           we could probably make it a bit
        //           more efficient than cloning these here.
        let mut filters = vec![];
        for filter in display_list.get(input_filters) {
            filters.push(filter);
        }
        filters
    }
}
