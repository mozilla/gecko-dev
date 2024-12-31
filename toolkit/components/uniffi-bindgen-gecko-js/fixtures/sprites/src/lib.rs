/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::RwLock;

// A point in two-dimensional space.
#[derive(Debug, Clone, uniffi::Record)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

// A magnitude and direction in two-dimensional space.
// For simplicity we represent this as a point relative to the origin.
#[derive(Debug, Clone, uniffi::Record)]
pub struct Vector {
    pub dx: f64,
    pub dy: f64,
}

// Move from the given Point, according to the given Vector.
#[uniffi::export]
pub fn translate(p: &Point, v: Vector) -> Point {
    Point {
        x: p.x + v.dx,
        y: p.y + v.dy,
    }
}

// An entity in our imaginary world, which occupies a position in space
// and which can move about over time.
#[derive(Debug, uniffi::Object)]
pub struct Sprite {
    // We must use interior mutability for managing mutable state, hence the `RwLock`.
    current_position: RwLock<Point>,
}

#[uniffi::export]
impl Sprite {
    #[uniffi::constructor]
    pub fn new(initial_position: Option<Point>) -> Sprite {
        Sprite {
            current_position: RwLock::new(initial_position.unwrap_or(Point { x: 0.0, y: 0.0 })),
        }
    }

    #[uniffi::constructor]
    pub fn new_relative_to(reference: Point, direction: Vector) -> Sprite {
        Sprite {
            current_position: RwLock::new(translate(&reference, direction)),
        }
    }

    pub fn get_position(&self) -> Point {
        self.current_position.read().unwrap().clone()
    }

    pub fn move_to(&self, position: Point) {
        *self.current_position.write().unwrap() = position;
    }

    pub fn move_by(&self, direction: Vector) {
        let mut current_position = self.current_position.write().unwrap();
        *current_position = translate(&current_position, direction)
    }
}

uniffi::setup_scaffolding!("sprites");
