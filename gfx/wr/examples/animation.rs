/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This example creates a 200x200 white rect and allows the user to move it
//! around by using the arrow keys and rotate with '<'/'>'.
//! It does this by using the animation API.

//! The example also features seamless opaque/transparent split of a
//! rounded cornered rectangle, which is done automatically during the
//! scene building for render optimization.

extern crate euclid;
extern crate gleam;
extern crate glutin;
extern crate webrender;
extern crate winit;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use boilerplate::{Example, HandyDandyRectBuilder};
use euclid::Angle;
use webrender::api::*;

struct App {
    property_key0: PropertyBindingKey<LayoutTransform>,
    property_key1: PropertyBindingKey<LayoutTransform>,
    property_key2: PropertyBindingKey<LayoutTransform>,
    opacity_key: PropertyBindingKey<f32>,
    opacity: f32,
    angle0: f32,
    angle1: f32,
    angle2: f32,
}

impl App {
    fn add_rounded_rect(
        &mut self,
        bounds: LayoutRect,
        color: ColorF,
        builder: &mut DisplayListBuilder,
        property_key: PropertyBindingKey<LayoutTransform>,
    ) {
        let filters = [
            FilterOp::Opacity(PropertyBinding::Binding(self.opacity_key, self.opacity), self.opacity),
        ];

        let reference_frame_id = builder.push_reference_frame(
            &LayoutPrimitiveInfo::new(LayoutRect::new(bounds.origin, LayoutSize::zero())),
            Some(PropertyBinding::Binding(property_key, LayoutTransform::identity())),
            None,
        );

        builder.push_clip_id(reference_frame_id);

        builder.push_stacking_context(
            &LayoutPrimitiveInfo::new(LayoutRect::zero()),
            None,
            TransformStyle::Flat,
            MixBlendMode::Normal,
            &filters,
            RasterSpace::Screen,
        );

        let clip_bounds = LayoutRect::new(LayoutPoint::zero(), bounds.size);
        let complex_clip = ComplexClipRegion {
            rect: clip_bounds,
            radii: BorderRadius::uniform(30.0),
            mode: ClipMode::Clip,
        };
        let clip_id = builder.define_clip(clip_bounds, vec![complex_clip], None);
        builder.push_clip_id(clip_id);

        // Fill it with a white rect
        builder.push_rect(
            &LayoutPrimitiveInfo::new(LayoutRect::new(LayoutPoint::zero(), bounds.size)),
            color,
        );

        builder.pop_clip_id();

        builder.pop_stacking_context();

        builder.pop_clip_id();
        builder.pop_reference_frame();
    }
}

impl Example for App {
    const WIDTH: u32 = 1024;
    const HEIGHT: u32 = 1024;

    fn render(
        &mut self,
        _api: &RenderApi,
        builder: &mut DisplayListBuilder,
        _txn: &mut Transaction,
        _framebuffer_size: DeviceIntSize,
        _pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        let bounds = (150, 150).to(250, 250);
        let key0 = self.property_key0;
        self.add_rounded_rect(bounds, ColorF::new(1.0, 0.0, 0.0, 0.5), builder, key0);

        let bounds = (400, 400).to(600, 600);
        let key1 = self.property_key1;
        self.add_rounded_rect(bounds, ColorF::new(0.0, 1.0, 0.0, 0.5), builder, key1);

        let bounds = (200, 500).to(350, 580);
        let key2 = self.property_key2;
        self.add_rounded_rect(bounds, ColorF::new(0.0, 0.0, 1.0, 0.5), builder, key2);
    }

    fn on_event(&mut self, win_event: winit::WindowEvent, api: &RenderApi, document_id: DocumentId) -> bool {
        match win_event {
            winit::WindowEvent::KeyboardInput {
                input: winit::KeyboardInput {
                    state: winit::ElementState::Pressed,
                    virtual_keycode: Some(key),
                    ..
                },
                ..
            } => {
                let (delta_angle, delta_opacity) = match key {
                    winit::VirtualKeyCode::Down => (0.0, -0.1),
                    winit::VirtualKeyCode::Up => (0.0, 0.1),
                    winit::VirtualKeyCode::Right => (1.0, 0.0),
                    winit::VirtualKeyCode::Left => (-1.0, 0.0),
                    _ => return false,
                };
                // Update the transform based on the keyboard input and push it to
                // webrender using the generate_frame API. This will recomposite with
                // the updated transform.
                self.opacity += delta_opacity;
                self.angle0 += delta_angle * 0.1;
                self.angle1 += delta_angle * 0.2;
                self.angle2 -= delta_angle * 0.15;
                let xf0 = LayoutTransform::create_rotation(0.0, 0.0, 1.0, Angle::radians(self.angle0));
                let xf1 = LayoutTransform::create_rotation(0.0, 0.0, 1.0, Angle::radians(self.angle1));
                let xf2 = LayoutTransform::create_rotation(0.0, 0.0, 1.0, Angle::radians(self.angle2));
                let mut txn = Transaction::new();
                txn.update_dynamic_properties(
                    DynamicProperties {
                        transforms: vec![
                            PropertyValue {
                                key: self.property_key0,
                                value: xf0,
                            },
                            PropertyValue {
                                key: self.property_key1,
                                value: xf1,
                            },
                            PropertyValue {
                                key: self.property_key2,
                                value: xf2,
                            },
                        ],
                        floats: vec![
                            PropertyValue {
                                key: self.opacity_key,
                                value: self.opacity,
                            }
                        ],
                    },
                );
                txn.generate_frame();
                api.send_transaction(document_id, txn);
            }
            _ => (),
        }

        false
    }
}

fn main() {
    let mut app = App {
        property_key0: PropertyBindingKey::new(42), // arbitrary magic number
        property_key1: PropertyBindingKey::new(44), // arbitrary magic number
        property_key2: PropertyBindingKey::new(45), // arbitrary magic number
        opacity_key: PropertyBindingKey::new(43),
        opacity: 0.5,
        angle0: 0.0,
        angle1: 0.0,
        angle2: 0.0,
    };
    boilerplate::main_wrapper(&mut app, None);
}
