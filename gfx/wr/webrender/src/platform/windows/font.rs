/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FontInstanceFlags, FontKey, FontRenderMode, FontVariation};
use api::{ColorU, GlyphDimensions};
use dwrote;
use gamma_lut::ColorLut;
use glyph_rasterizer::{FontInstance, FontTransform, GlyphKey};
use internal_types::{FastHashMap, ResourceCacheError};
use std::collections::hash_map::Entry;
use std::sync::Arc;
cfg_if! {
    if #[cfg(feature = "pathfinder")] {
        use pathfinder_font_renderer::{PathfinderComPtr, IDWriteFontFace};
        use glyph_rasterizer::NativeFontHandleWrapper;
    } else if #[cfg(not(feature = "pathfinder"))] {
        use api::FontInstancePlatformOptions;
        use glyph_rasterizer::{GlyphFormat, GlyphRasterResult, RasterizedGlyph};
        use gamma_lut::GammaLut;
    }
}

lazy_static! {
    static ref DEFAULT_FONT_DESCRIPTOR: dwrote::FontDescriptor = dwrote::FontDescriptor {
        family_name: "Arial".to_owned(),
        weight: dwrote::FontWeight::Regular,
        stretch: dwrote::FontStretch::Normal,
        style: dwrote::FontStyle::Normal,
    };
}

pub struct FontContext {
    fonts: FastHashMap<FontKey, dwrote::FontFace>,
    variations: FastHashMap<(FontKey, dwrote::DWRITE_FONT_SIMULATIONS, Vec<FontVariation>), dwrote::FontFace>,
    #[cfg(not(feature = "pathfinder"))]
    gamma_luts: FastHashMap<(u16, u16), GammaLut>,
}

// DirectWrite is safe to use on multiple threads and non-shareable resources are
// all hidden inside their font context.
unsafe impl Send for FontContext {}

fn dwrite_texture_type(render_mode: FontRenderMode) -> dwrote::DWRITE_TEXTURE_TYPE {
    match render_mode {
        FontRenderMode::Mono => dwrote::DWRITE_TEXTURE_ALIASED_1x1,
        FontRenderMode::Alpha |
        FontRenderMode::Subpixel => dwrote::DWRITE_TEXTURE_CLEARTYPE_3x1,
    }
}

fn dwrite_measure_mode(
    font: &FontInstance,
    bitmaps: bool,
) -> dwrote::DWRITE_MEASURING_MODE {
    if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
        dwrote::DWRITE_MEASURING_MODE_GDI_CLASSIC
    } else {
      match font.render_mode {
          FontRenderMode::Mono => dwrote::DWRITE_MEASURING_MODE_GDI_CLASSIC,
          FontRenderMode::Alpha | FontRenderMode::Subpixel => dwrote::DWRITE_MEASURING_MODE_NATURAL,
      }
    }
}

fn dwrite_render_mode(
    font_face: &dwrote::FontFace,
    font: &FontInstance,
    em_size: f32,
    measure_mode: dwrote::DWRITE_MEASURING_MODE,
    bitmaps: bool,
) -> dwrote::DWRITE_RENDERING_MODE {
    let dwrite_render_mode = match font.render_mode {
        FontRenderMode::Mono => dwrote::DWRITE_RENDERING_MODE_ALIASED,
        FontRenderMode::Alpha | FontRenderMode::Subpixel => {
            if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
                dwrote::DWRITE_RENDERING_MODE_GDI_CLASSIC
            } else {
                font_face.get_recommended_rendering_mode_default_params(em_size, 1.0, measure_mode)
            }
        }
    };

    if dwrite_render_mode == dwrote::DWRITE_RENDERING_MODE_OUTLINE {
        // Outline mode is not supported
        return dwrote::DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC;
    }

    dwrite_render_mode
}

fn is_bitmap_font(font: &FontInstance) -> bool {
    // If bitmaps are requested, then treat as a bitmap font to disable transforms.
    // If mono AA is requested, let that take priority over using bitmaps.
    font.render_mode != FontRenderMode::Mono &&
        font.flags.contains(FontInstanceFlags::EMBEDDED_BITMAPS)
}

impl FontContext {
    pub fn new() -> Result<FontContext, ResourceCacheError> {
        Ok(FontContext {
            fonts: FastHashMap::default(),
            variations: FastHashMap::default(),
            #[cfg(not(feature = "pathfinder"))]
            gamma_luts: FastHashMap::default(),
        })
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.fonts.contains_key(font_key)
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, data: Arc<Vec<u8>>, index: u32) {
        if self.fonts.contains_key(font_key) {
            return;
        }

        if let Some(font_file) = dwrote::FontFile::new_from_data(data) {
            let face = font_file.create_face(index, dwrote::DWRITE_FONT_SIMULATIONS_NONE);
            self.fonts.insert(*font_key, face);
        } else {
            // XXX add_raw_font needs to have a way to return an error
            debug!("DWrite WR failed to load font from data, using Arial instead");
            self.add_native_font(font_key, DEFAULT_FONT_DESCRIPTOR.clone());
        }
    }

    pub fn load_system_font(font_handle: &dwrote::FontDescriptor, update: bool) -> Result<dwrote::Font, String> {
        let system_fc = dwrote::FontCollection::get_system(update);
        // A version of get_font_from_descriptor() that panics early to help with bug 1455848
        if let Some(family) = system_fc.get_font_family_by_name(&font_handle.family_name) {
            let font = family.get_first_matching_font(font_handle.weight, font_handle.stretch, font_handle.style);
            // Exact matches only here
            if font.weight() == font_handle.weight &&
                font.stretch() == font_handle.stretch &&
                font.style() == font_handle.style
            {
                Ok(font)
            } else {
                // We can't depend on the family's fonts being in a particular order, so the first match may not
                // be an exact match, even though it is sufficiently close to be a match. As a slower fallback,
                // try looking through all of the fonts in the family for an exact match. The caller should have
                // verified that an exact match exists so that this search shouldn't fail.
                (0 .. family.get_font_count()).filter_map(|idx| {
                    let alt = family.get_font(idx);
                    if alt.weight() == font_handle.weight &&
                        alt.stretch() == font_handle.stretch &&
                        alt.style() == font_handle.style
                    {
                        Some(alt)
                    } else {
                        None
                    }
                }).next().ok_or_else(|| {
                    format!("font mismatch for descriptor {:?} {:?}", font_handle, font.to_descriptor())
                })
            }
        } else {
            Err(format!("missing font family for descriptor {:?}", font_handle))
        }
    }

    pub fn add_native_font(&mut self, font_key: &FontKey, font_handle: dwrote::FontDescriptor) {
        if self.fonts.contains_key(font_key) {
            return;
        }
        // First try to load the font without updating the system font collection.
        // If the font can't be found, try again after updating the system font collection.
        // If even that fails, panic...
        let font = Self::load_system_font(&font_handle, false).unwrap_or_else(|_| {
            Self::load_system_font(&font_handle, true).unwrap()
        });
        let face = font.create_font_face();
        self.fonts.insert(*font_key, face);
    }

    pub fn delete_font(&mut self, font_key: &FontKey) {
        if let Some(_) = self.fonts.remove(font_key) {
            self.variations.retain(|k, _| k.0 != *font_key);
        }
    }

    pub fn delete_font_instance(&mut self, instance: &FontInstance) {
        // Ensure we don't keep around excessive amounts of stale variations.
        if !instance.variations.is_empty() {
            let sims = if instance.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
                dwrote::DWRITE_FONT_SIMULATIONS_BOLD
            } else {
                dwrote::DWRITE_FONT_SIMULATIONS_NONE
            };
            self.variations.remove(&(instance.font_key, sims, instance.variations.clone()));
        }
    }

    // Assumes RGB format from dwrite, which is 3 bytes per pixel as dwrite
    // doesn't output an alpha value via GlyphRunAnalysis::CreateAlphaTexture
    #[allow(dead_code)]
    fn print_glyph_data(&self, data: &[u8], width: usize, height: usize) {
        // Rust doesn't have step_by support on stable :(
        for i in 0 .. height {
            let current_height = i * width * 3;

            for pixel in data[current_height .. current_height + (width * 3)].chunks(3) {
                let r = pixel[0];
                let g = pixel[1];
                let b = pixel[2];
                print!("({}, {}, {}) ", r, g, b,);
            }
            println!("");
        }
    }

    fn get_font_face(
        &mut self,
        font: &FontInstance,
    ) -> &dwrote::FontFace {
        if !font.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) &&
           font.variations.is_empty() {
            return self.fonts.get(&font.font_key).unwrap();
        }
        let sims = if font.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
            dwrote::DWRITE_FONT_SIMULATIONS_BOLD
        } else {
            dwrote::DWRITE_FONT_SIMULATIONS_NONE
        };
        match self.variations.entry((font.font_key, sims, font.variations.clone())) {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => {
                let normal_face = self.fonts.get(&font.font_key).unwrap();
                if !font.variations.is_empty() {
                    if let Some(var_face) = normal_face.create_font_face_with_variations(
                        sims,
                        &font.variations.iter().map(|var| {
                            dwrote::DWRITE_FONT_AXIS_VALUE {
                                // OpenType tags are big-endian, but DWrite wants little-endian.
                                axisTag: var.tag.swap_bytes(),
                                value: var.value,
                            }
                        }).collect::<Vec<_>>(),
                    ) {
                        return entry.insert(var_face);
                    }
                }
                entry.insert(normal_face.create_font_face_with_simulations(sims))
            }
        }
    }

    fn create_glyph_analysis(
        &mut self,
        font: &FontInstance,
        key: &GlyphKey,
        size: f32,
        transform: Option<dwrote::DWRITE_MATRIX>,
        bitmaps: bool,
    ) -> dwrote::GlyphRunAnalysis {
        let face = self.get_font_face(font);
        let glyph = key.index() as u16;
        let advance = 0.0f32;
        let offset = dwrote::GlyphOffset {
            advanceOffset: 0.0,
            ascenderOffset: 0.0,
        };

        let glyph_run = dwrote::DWRITE_GLYPH_RUN {
            fontFace: unsafe { face.as_ptr() },
            fontEmSize: size, // size in DIPs (1/96", same as CSS pixels)
            glyphCount: 1,
            glyphIndices: &glyph,
            glyphAdvances: &advance,
            glyphOffsets: &offset,
            isSideways: 0,
            bidiLevel: 0,
        };

        let dwrite_measure_mode = dwrite_measure_mode(font, bitmaps);
        let dwrite_render_mode = dwrite_render_mode(
            face,
            font,
            size,
            dwrite_measure_mode,
            bitmaps,
        );

        dwrote::GlyphRunAnalysis::create(
            &glyph_run,
            1.0,
            transform,
            dwrite_render_mode,
            dwrite_measure_mode,
            0.0,
            0.0,
        )
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        let face = self.fonts.get(&font_key).unwrap();
        let indices = face.get_glyph_indices(&[ch as u32]);
        indices.first().map(|idx| *idx as u32)
    }

    pub fn get_glyph_dimensions(
        &mut self,
        font: &FontInstance,
        key: &GlyphKey,
    ) -> Option<GlyphDimensions> {
        let size = font.size.to_f32_px();
        let bitmaps = is_bitmap_font(font);
        let transform = if font.synthetic_italics.is_enabled() ||
                           font.flags.intersects(FontInstanceFlags::TRANSPOSE |
                                                 FontInstanceFlags::FLIP_X |
                                                 FontInstanceFlags::FLIP_Y) {
            let mut shape = FontTransform::identity();
            if font.flags.contains(FontInstanceFlags::FLIP_X) {
                shape = shape.flip_x();
            }
            if font.flags.contains(FontInstanceFlags::FLIP_Y) {
                shape = shape.flip_y();
            }
            if font.flags.contains(FontInstanceFlags::TRANSPOSE) {
                shape = shape.swap_xy();
            }
            if font.synthetic_italics.is_enabled() {
                shape = shape.synthesize_italics(font.synthetic_italics);
            }
            Some(dwrote::DWRITE_MATRIX {
                m11: shape.scale_x,
                m12: shape.skew_y,
                m21: shape.skew_x,
                m22: shape.scale_y,
                dx: 0.0,
                dy: 0.0,
            })
        } else {
            None
        };
        let analysis = self.create_glyph_analysis(font, key, size, transform, bitmaps);

        let texture_type = dwrite_texture_type(font.render_mode);

        let bounds = analysis.get_alpha_texture_bounds(texture_type);

        let width = (bounds.right - bounds.left) as i32;
        let height = (bounds.bottom - bounds.top) as i32;

        // Alpha texture bounds can sometimes return an empty rect
        // Such as for spaces
        if width == 0 || height == 0 {
            return None;
        }

        let face = self.get_font_face(font);
        face.get_design_glyph_metrics(&[key.index() as u16], false)
            .first()
            .map(|metrics| {
                let em_size = size / 16.;
                let design_units_per_pixel = face.metrics().designUnitsPerEm as f32 / 16. as f32;
                let scaled_design_units_to_pixels = em_size / design_units_per_pixel;
                let advance = metrics.advanceWidth as f32 * scaled_design_units_to_pixels;

                GlyphDimensions {
                    left: bounds.left,
                    top: -bounds.top,
                    width,
                    height,
                    advance: advance,
                }
            })
    }

    // DWrite ClearType gives us values in RGB, but WR expects BGRA.
    #[cfg(not(feature = "pathfinder"))]
    fn convert_to_bgra(
        &self,
        pixels: &[u8],
        render_mode: FontRenderMode,
        bitmaps: bool,
    ) -> Vec<u8> {
        match (render_mode, bitmaps) {
            (FontRenderMode::Mono, _) => {
                let mut bgra_pixels: Vec<u8> = vec![0; pixels.len() * 4];
                for i in 0 .. pixels.len() {
                    let alpha = pixels[i];
                    bgra_pixels[i * 4 + 0] = alpha;
                    bgra_pixels[i * 4 + 1] = alpha;
                    bgra_pixels[i * 4 + 2] = alpha;
                    bgra_pixels[i * 4 + 3] = alpha;
                }
                bgra_pixels
            }
            (FontRenderMode::Alpha, _) | (_, true) => {
                let length = pixels.len() / 3;
                let mut bgra_pixels: Vec<u8> = vec![0; length * 4];
                for i in 0 .. length {
                    // Only take the G channel, as its closest to D2D
                    let alpha = pixels[i * 3 + 1] as u8;
                    bgra_pixels[i * 4 + 0] = alpha;
                    bgra_pixels[i * 4 + 1] = alpha;
                    bgra_pixels[i * 4 + 2] = alpha;
                    bgra_pixels[i * 4 + 3] = alpha;
                }
                bgra_pixels
            }
            (FontRenderMode::Subpixel, false) => {
                let length = pixels.len() / 3;
                let mut bgra_pixels: Vec<u8> = vec![0; length * 4];
                for i in 0 .. length {
                    bgra_pixels[i * 4 + 0] = pixels[i * 3 + 2];
                    bgra_pixels[i * 4 + 1] = pixels[i * 3 + 1];
                    bgra_pixels[i * 4 + 2] = pixels[i * 3 + 0];
                    bgra_pixels[i * 4 + 3] = 0xff;
                }
                bgra_pixels
            }
        }
    }

    pub fn prepare_font(font: &mut FontInstance) {
        match font.render_mode {
            FontRenderMode::Mono => {
                // In mono mode the color of the font is irrelevant.
                font.color = ColorU::new(255, 255, 255, 255);
                // Subpixel positioning is disabled in mono mode.
                font.disable_subpixel_position();
            }
            FontRenderMode::Alpha => {
                font.color = font.color.luminance_color().quantize();
            }
            FontRenderMode::Subpixel => {
                font.color = font.color.quantize();
            }
        }
    }

    #[cfg(not(feature = "pathfinder"))]
    pub fn rasterize_glyph(&mut self, font: &FontInstance, key: &GlyphKey) -> GlyphRasterResult {
        let (x_scale, y_scale) = font.transform.compute_scale().unwrap_or((1.0, 1.0));
        let scale = font.oversized_scale_factor(x_scale, y_scale);
        let size = (font.size.to_f64_px() * y_scale / scale) as f32;
        let bitmaps = is_bitmap_font(font);
        let (mut shape, (x_offset, y_offset)) = if bitmaps {
            (FontTransform::identity(), (0.0, 0.0))
        } else {
            (font.transform.invert_scale(y_scale, y_scale), font.get_subpx_offset(key))
        };
        if font.flags.contains(FontInstanceFlags::FLIP_X) {
            shape = shape.flip_x();
        }
        if font.flags.contains(FontInstanceFlags::FLIP_Y) {
            shape = shape.flip_y();
        }
        if font.flags.contains(FontInstanceFlags::TRANSPOSE) {
            shape = shape.swap_xy();
        }
        if font.synthetic_italics.is_enabled() {
            shape = shape.synthesize_italics(font.synthetic_italics);
        }
        let transform = if !shape.is_identity() || (x_offset, y_offset) != (0.0, 0.0) {
            Some(dwrote::DWRITE_MATRIX {
                m11: shape.scale_x,
                m12: shape.skew_y,
                m21: shape.skew_x,
                m22: shape.scale_y,
                dx: (x_offset / scale) as f32,
                dy: (y_offset / scale) as f32,
            })
        } else {
            None
        };

        let analysis = self.create_glyph_analysis(font, key, size, transform, bitmaps);
        let texture_type = dwrite_texture_type(font.render_mode);

        let bounds = analysis.get_alpha_texture_bounds(texture_type);
        let width = (bounds.right - bounds.left) as i32;
        let height = (bounds.bottom - bounds.top) as i32;

        // Alpha texture bounds can sometimes return an empty rect
        // Such as for spaces
        if width == 0 || height == 0 {
            return GlyphRasterResult::LoadFailed;
        }

        let pixels = analysis.create_alpha_texture(texture_type, bounds);
        let mut bgra_pixels = self.convert_to_bgra(&pixels, font.render_mode, bitmaps);

        // These are the default values we use in Gecko.
        // We use a gamma value of 2.3 for gdi fonts
        const GDI_GAMMA: u16 = 230;

        let FontInstancePlatformOptions { gamma, contrast, .. } = font.platform_options.unwrap_or_default();
        let gdi_gamma = match font.render_mode {
            FontRenderMode::Mono => GDI_GAMMA,
            FontRenderMode::Alpha | FontRenderMode::Subpixel => {
                if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
                    GDI_GAMMA
                } else {
                    gamma
                }
            }
        };
        let gamma_lut = self.gamma_luts
            .entry((gdi_gamma, contrast))
            .or_insert_with(||
                GammaLut::new(
                    contrast as f32 / 100.0,
                    gdi_gamma as f32 / 100.0,
                    gdi_gamma as f32 / 100.0,
                ));
        gamma_lut.preblend(&mut bgra_pixels, font.color);

        GlyphRasterResult::Bitmap(RasterizedGlyph {
            left: bounds.left as f32,
            top: -bounds.top as f32,
            width,
            height,
            scale: (if bitmaps { scale / y_scale } else { scale }) as f32,
            format: if bitmaps { GlyphFormat::Bitmap } else { font.get_glyph_format() },
            bytes: bgra_pixels,
        })
    }
}

#[cfg(feature = "pathfinder")]
impl<'a> From<NativeFontHandleWrapper<'a>> for PathfinderComPtr<IDWriteFontFace> {
    fn from(font_handle: NativeFontHandleWrapper<'a>) -> Self {
        let system_fc = ::dwrote::FontCollection::system();
        let font = match system_fc.get_font_from_descriptor(&font_handle.0) {
            Some(font) => font,
            None => panic!("missing descriptor {:?}", font_handle.0),
        };
        let face = font.create_font_face();
        unsafe { PathfinderComPtr::new(face.as_ptr()) }
    }
}
