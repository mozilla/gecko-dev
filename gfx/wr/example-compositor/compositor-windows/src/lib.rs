/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::os::raw::{c_void, c_char};

/*

  This is a very simple (and unsafe!) rust wrapper for the DirectComposite / D3D11 / ANGLE
  implementation in lib.cpp.

  It just proxies the calls from the Compositor impl to the C99 code. This is very
  hacky and not suitable for production!

 */

// Opaque wrapper for the Window type in lib.cpp
#[repr(C)]
pub struct Window {
    _unused: [u8; 0]
}

// C99 functions that do the compositor work
extern {
    fn com_dc_create_window(width: i32, height: i32, enable_compositor: bool) -> *mut Window;
    fn com_dc_destroy_window(window: *mut Window);
    fn com_dc_tick(window: *mut Window) -> bool;
    fn com_dc_get_proc_address(name: *const c_char) -> *const c_void;
    fn com_dc_swap_buffers(window: *mut Window);

    fn com_dc_create_surface(
        window: *mut Window,
        id: u64,
        width: i32,
        height: i32,
    );

    fn com_dc_destroy_surface(
        window: *mut Window,
        id: u64,
    );

    fn com_dc_bind_surface(
        window: *mut Window,
        id: u64,
        x_offset: &mut i32,
        y_offset: &mut i32,
    );
    fn com_dc_unbind_surface(window: *mut Window);

    fn com_dc_begin_transaction(window: *mut Window);

    fn com_dc_add_surface(
        window: *mut Window,
        id: u64,
        x: i32,
        y: i32,
        clip_x: i32,
        clip_y: i32,
        clip_w: i32,
        clip_h: i32,
    );

    fn com_dc_end_transaction(window: *mut Window);
}

pub fn create_window(width: i32, height: i32, enable_compositor: bool) -> *mut Window {
    unsafe {
        com_dc_create_window(width, height, enable_compositor)
    }
}

pub fn destroy_window(window: *mut Window) {
    unsafe {
        com_dc_destroy_window(window);
    }
}

pub fn tick(window: *mut Window) -> bool {
    unsafe {
        com_dc_tick(window)
    }
}

pub fn get_proc_address(name: *const c_char) -> *const c_void {
    unsafe {
        com_dc_get_proc_address(name)
    }
}

pub fn create_surface(
    window: *mut Window,
    id: u64,
    width: i32,
    height: i32,
) {
    unsafe {
        com_dc_create_surface(
            window,
            id,
            width,
            height
        )
    }
}

pub fn destroy_surface(
    window: *mut Window,
    id: u64,
) {
    unsafe {
        com_dc_destroy_surface(
            window,
            id,
        )
    }
}

pub fn bind_surface(
    window: *mut Window,
    id: u64,
) -> (i32, i32) {
    unsafe {
        let mut x_offset = 0;
        let mut y_offset = 0;

        com_dc_bind_surface(
            window,
            id,
            &mut x_offset,
            &mut y_offset,
        );

        (x_offset, y_offset)
    }
}

pub fn add_surface(
    window: *mut Window,
    id: u64,
    x: i32,
    y: i32,
    clip_x: i32,
    clip_y: i32,
    clip_w: i32,
    clip_h: i32,
) {
    unsafe {
        com_dc_add_surface(
            window,
            id,
            x,
            y,
            clip_x,
            clip_y,
            clip_w,
            clip_h,
        )
    }
}

pub fn begin_transaction(window: *mut Window) {
    unsafe {
        com_dc_begin_transaction(window)
    }
}

pub fn unbind_surface(window: *mut Window) {
    unsafe {
        com_dc_unbind_surface(window)
    }
}

pub fn end_transaction(window: *mut Window) {
    unsafe {
        com_dc_end_transaction(window)
    }
}

pub fn swap_buffers(window: *mut Window) {
    unsafe {
        com_dc_swap_buffers(window);
    }
}
