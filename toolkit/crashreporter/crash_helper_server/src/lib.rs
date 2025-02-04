/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(any(target_os = "linux", target_os = "android"))]
extern crate rust_minidump_writer_linux;

mod breakpad_crash_generator;
mod crash_generation;
mod ipc_server;
mod phc;

use cfg_if::cfg_if;
use crash_helper_common::Pid;
use std::{
    ffi::{c_char, CStr},
    path::PathBuf,
    str::FromStr,
};

use crash_generation::CrashGenerator;
use ipc_server::{IPCServer, IPCServerState};

#[no_mangle]
pub extern "C" fn crash_generator_logic(client_pid: Pid) -> i32 {
    let crash_generator = CrashGenerator::new(client_pid)
        .unwrap();

    let ipc_server = IPCServer::new(client_pid)
        .unwrap();

    cfg_if! {
        if #[cfg(target_os = "android")] {
            // On Android the main thread is used to respond to the intents so
            // we can't block it. Run the crash generation loop in a separate
            // thread.
            let _ = std::thread::spawn(move || {
                main_loop(ipc_server, crash_generator)
            });

            0
        } else {
            main_loop(ipc_server, crash_generator)
        }
    }
}

fn main_loop(mut ipc_server: IPCServer, mut crash_generator: CrashGenerator) -> i32 {
    loop {
        match ipc_server.run(&mut crash_generator) {
            Ok(_result @ IPCServerState::ClientDisconnected) => {
                return 0;
            }
            Err(error) => {
                return -1;
            }
            _ => {} // Go on
        }
    }
}
