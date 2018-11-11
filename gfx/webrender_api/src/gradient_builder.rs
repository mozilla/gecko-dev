/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {ExtendMode, Gradient, GradientStop, LayoutPoint, LayoutSize, RadialGradient};


/// Construct a gradient to be used in display lists.
///
/// Each gradient needs at least two stops.
pub struct GradientBuilder {
    stops: Vec<GradientStop>,
}

impl GradientBuilder {
    /// Create a new gradient builder.
    pub fn new() -> GradientBuilder {
        GradientBuilder {
            stops: Vec::new(),
        }
    }

    /// Create a gradient builder with a list of stops.
    pub fn with_stops(stops: Vec<GradientStop>) -> GradientBuilder {
        GradientBuilder { stops }
    }

    /// Push an additional stop for the gradient.
    pub fn push(&mut self, stop: GradientStop) {
        self.stops.push(stop);
    }

    /// Get a reference to the list of stops.
    pub fn stops(&self) -> &[GradientStop] {
        self.stops.as_ref()
    }

    /// Return the gradient stops vector.
    pub fn into_stops(self) -> Vec<GradientStop> {
        self.stops
    }

    /// Produce a linear gradient, normalize the stops.
    pub fn gradient(
        &mut self,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        extend_mode: ExtendMode,
    ) -> Gradient {
        let (start_offset, end_offset) = self.normalize(extend_mode);
        let start_to_end = end_point - start_point;

        Gradient {
            start_point: start_point + start_to_end * start_offset,
            end_point: start_point + start_to_end * end_offset,
            extend_mode,
        }
    }

    /// Produce a radial gradient, normalize the stops.
    ///
    /// Will replace the gradient with a single color
    /// if the radius negative.
    pub fn radial_gradient(
        &mut self,
        center: LayoutPoint,
        radius: LayoutSize,
        extend_mode: ExtendMode,
    ) -> RadialGradient {
        if radius.width <= 0.0 || radius.height <= 0.0 {
            // The shader cannot handle a non positive radius. So
            // reuse the stops vector and construct an equivalent
            // gradient.
            let last_color = self.stops.last().unwrap().color;

            self.stops.clear();
            self.stops.push(GradientStop { offset: 0.0, color: last_color, });
            self.stops.push(GradientStop { offset: 1.0, color: last_color, });

            return RadialGradient {
                center,
                radius: LayoutSize::new(1.0, 1.0),
                start_offset: 0.0,
                end_offset: 1.0,
                extend_mode,
            };
        }

        let (start_offset, end_offset) =
            self.normalize(extend_mode);

        RadialGradient {
            center,
            radius,
            start_offset,
            end_offset,
            extend_mode,
        }
    }

    /// Gradients can be defined with stops outside the range of [0, 1]
    /// when this happens the gradient needs to be normalized by adjusting
    /// the gradient stops and gradient line into an equivalent gradient
    /// with stops in the range [0, 1]. this is done by moving the beginning
    /// of the gradient line to where stop[0] and the end of the gradient line
    /// to stop[n-1]. this function adjusts the stops in place, and returns
    /// the amount to adjust the gradient line start and stop.
    fn normalize(&mut self, extend_mode: ExtendMode) -> (f32, f32) {
        let stops = &mut self.stops;
        assert!(stops.len() >= 2);

        let first = *stops.first().unwrap();
        let last = *stops.last().unwrap();

        assert!(first.offset <= last.offset);

        let stops_delta = last.offset - first.offset;

        if stops_delta > 0.000001 {
            for stop in stops {
                stop.offset = (stop.offset - first.offset) / stops_delta;
            }

            (first.offset, last.offset)
        } else {
            // We have a degenerate gradient and can't accurately transform the stops
            // what happens here depends on the repeat behavior, but in any case
            // we reconstruct the gradient stops to something simpler and equivalent
            stops.clear();

            match extend_mode {
                ExtendMode::Clamp => {
                    // This gradient is two colors split at the offset of the stops,
                    // so create a gradient with two colors split at 0.5 and adjust
                    // the gradient line so 0.5 is at the offset of the stops
                    stops.push(GradientStop { color: first.color, offset: 0.0, });
                    stops.push(GradientStop { color: first.color, offset: 0.5, });
                    stops.push(GradientStop { color: last.color, offset: 0.5, });
                    stops.push(GradientStop { color: last.color, offset: 1.0, });

                    let offset = last.offset;

                    (offset - 0.5, offset + 0.5)
                }
                ExtendMode::Repeat => {
                    // A repeating gradient with stops that are all in the same
                    // position should just display the last color. I believe the
                    // spec says that it should be the average color of the gradient,
                    // but this matches what Gecko and Blink does
                    stops.push(GradientStop { color: last.color, offset: 0.0, });
                    stops.push(GradientStop { color: last.color, offset: 1.0, });

                    (0.0, 1.0)
                }
            }
        }
    }
}
