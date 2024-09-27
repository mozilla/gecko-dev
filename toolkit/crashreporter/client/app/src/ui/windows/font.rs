/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::Dpi;
use super::WideString;
use windows_sys::Win32::{
    Foundation::{ERROR_SUCCESS, S_OK},
    Graphics::Gdi,
    System::Registry,
    UI::Controls,
};

/// The default font size to use.
///
/// `GetThemeSysFont` scales the font based on DPI and accessibility settings, however it only takes
/// those active at application startup into account (it won't scale correctly across monitors, nor
/// if the DPI of the current monitor changes). So we use a fixed size instead and scale that.
const DEFAULT_FONT_SIZE: i32 = -12;

/// The set of fonts to use.
pub struct Fonts {
    pub normal: Font,
    pub bold: Font,
}

/// The scale factor set by the windows 10 "make text bigger" accessibility setting.
#[derive(Clone, Copy, Debug)]
pub struct ScaleFactor(f32);

impl ScaleFactor {
    /// Create a new scale factor.
    ///
    /// The factor will be clamped to [1,2.25], to match the
    /// [documentation](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.uisettings.textscalefactor)
    /// and avoid any surprises.
    pub fn new(factor: f32) -> Self {
        ScaleFactor(factor.clamp(1., 2.25))
    }

    /// Get the current scale factor setting from the registry.
    pub fn from_registry() -> Self {
        let key = WideString::new("SOFTWARE\\Microsoft\\Accessibility");
        let value = WideString::new("TextScaleFactor");
        let mut scale_factor: [u8; 4] = Default::default();
        let mut size: u32 = std::mem::size_of_val(&scale_factor) as u32;
        let mut reg_type: u32 = 0;
        let result = unsafe {
            Registry::RegGetValueW(
                Registry::HKEY_CURRENT_USER,
                key.pcwstr(),
                value.pcwstr(),
                Registry::RRF_RT_REG_DWORD,
                &mut reg_type,
                &mut scale_factor as *mut u8 as _,
                &mut size,
            )
        };
        let percent = if result == ERROR_SUCCESS {
            if reg_type == Registry::REG_DWORD_BIG_ENDIAN {
                u32::from_be_bytes(scale_factor)
            } else {
                u32::from_le_bytes(scale_factor)
            }
        } else {
            100
        };
        Self::new(percent as f32 / 100.)
    }
}

impl Fonts {
    pub fn new(dpi: Dpi, scale_factor: ScaleFactor) -> Self {
        let builder = FontBuilder { dpi, scale_factor };
        Fonts {
            normal: builder.caption(),
            bold: builder.caption_bold().unwrap_or_else(|| builder.caption()),
        }
    }
}

pub struct FontBuilder {
    dpi: Dpi,
    scale_factor: ScaleFactor,
}

impl FontBuilder {
    /// Get the system theme caption font.
    ///
    /// Panics if the font cannot be retrieved.
    pub fn caption(&self) -> Font {
        unsafe {
            let mut font = std::mem::zeroed::<Gdi::LOGFONTW>();
            success!(hresult
                Controls::GetThemeSysFont(0, Controls::TMT_CAPTIONFONT as i32, &mut font)
            );
            font.lfHeight = DEFAULT_FONT_SIZE;
            self.scale_font_height(&mut font.lfHeight);
            font.lfWidth = 0;
            Font(success!(pointer Gdi::CreateFontIndirectW(&font)))
        }
    }

    /// Get the system theme bold caption font.
    ///
    /// Returns `None` if the font cannot be retrieved.
    pub fn caption_bold(&self) -> Option<Font> {
        unsafe {
            let mut font = std::mem::zeroed::<Gdi::LOGFONTW>();
            if Controls::GetThemeSysFont(0, Controls::TMT_CAPTIONFONT as i32, &mut font) != S_OK {
                return None;
            }
            font.lfHeight = DEFAULT_FONT_SIZE;
            self.scale_font_height(&mut font.lfHeight);
            font.lfWidth = 0;
            font.lfWeight = Gdi::FW_BOLD as i32;

            let ptr = Gdi::CreateFontIndirectW(&font);
            if ptr == 0 {
                return None;
            }
            Some(Font(ptr))
        }
    }

    fn scale_font_height(&self, height: &mut i32) {
        *height = (self.dpi.scale(height.abs() as u32) as f32 * self.scale_factor.0) as i32
            * if height.is_negative() { -1 } else { 1 };
    }
}

/// Windows font handle (`HFONT`).
pub struct Font(Gdi::HFONT);

impl std::ops::Deref for Font {
    type Target = Gdi::HFONT;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Drop for Font {
    fn drop(&mut self) {
        unsafe { Gdi::DeleteObject(self.0 as _) };
    }
}
