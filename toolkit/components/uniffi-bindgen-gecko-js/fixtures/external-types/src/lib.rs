/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::Arc;

use uniffi_geometry::{Line, Point};

#[uniffi::export]
pub fn gradient(value: Option<Line>) -> f64 {
    match value {
        None => 0.0,
        Some(value) => uniffi_geometry::gradient(value),
    }
}

#[uniffi::export]
pub fn intersection(ln1: Line, ln2: Line) -> Option<Point> {
    uniffi_geometry::intersection(ln1, ln2)
}

#[uniffi::export]
pub fn move_sprite_to_origin(sprite: Arc<uniffi_sprites::Sprite>) {
    sprite.move_to(uniffi_sprites::Point { x: 0.0, y: 0.0 })
}

uniffi::setup_scaffolding!("external_types");
