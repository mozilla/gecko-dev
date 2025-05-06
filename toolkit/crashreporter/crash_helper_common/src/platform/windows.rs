/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{errors::IPCError, Pid};
use std::{
    os::windows::io::{AsRawHandle, BorrowedHandle, FromRawHandle, OwnedHandle, RawHandle},
    ptr::null,
};
use windows_sys::Win32::{
    Foundation::{GetLastError, ERROR_NOT_FOUND, FALSE, HANDLE, TRUE},
    System::{
        Threading::{CreateEventA, ResetEvent, SetEvent},
        IO::{CancelIoEx, GetOverlappedResult, OVERLAPPED},
    },
};

pub(crate) fn server_name(pid: Pid) -> String {
    // We'll be passing this to CreateNamedPipeA() so we nul-terminate it.
    format!("\\\\.\\pipe\\gecko-crash-helper-pipe.{pid:}\0")
}

pub(crate) fn create_manual_reset_event() -> Result<OwnedHandle, IPCError> {
    // SAFETY: We pass null pointers for all the pointer arguments.
    let raw_handle = unsafe {
        CreateEventA(
            /* lpEventAttributes */ null(),
            /* bManualReset */ FALSE,
            /* bInitialState */ FALSE,
            /* lpName */ null(),
        )
    } as RawHandle;

    if raw_handle.is_null() {
        return Err(IPCError::System(unsafe { GetLastError() }));
    }

    // SAFETY: We just verified that `raw_handle` is valid.
    Ok(unsafe { OwnedHandle::from_raw_handle(raw_handle) })
}

pub(crate) fn reset_event(handle: BorrowedHandle) -> Result<(), IPCError> {
    // SAFETY: The handle we pass is guaranteed to be valid.
    let res = unsafe { ResetEvent(handle.as_raw_handle() as HANDLE) };

    match res {
        FALSE => Err(IPCError::System(unsafe { GetLastError() })),
        _ => Ok(()),
    }
}

pub(crate) fn set_event(handle: BorrowedHandle) -> Result<(), IPCError> {
    // SAFETY: The handle we pass is guaranteed to be valid.
    let res = unsafe { SetEvent(handle.as_raw_handle() as HANDLE) };

    match res {
        FALSE => Err(IPCError::System(unsafe { GetLastError() })),
        _ => Ok(()),
    }
}

pub(crate) fn cancel_overlapped_io(handle: BorrowedHandle, mut overlapped: Box<OVERLAPPED>) {
    let res = unsafe { CancelIoEx(handle.as_raw_handle() as HANDLE, overlapped.as_mut()) };
    if res == 0 {
        if unsafe { GetLastError() } != ERROR_NOT_FOUND {
            // If we get here an asynchronous I/O operation was pending
            // and we failed to cancel it. Leak the corresponding
            // OVERLAPPED structure instead since it could still
            // complete sometimes in the future.
            let _ = Box::leak(overlapped);
        }

        return;
    }

    // Just wait for the operation to finish, we don't care about the result
    let mut number_of_bytes_transferred: u32 = 0;
    let res = unsafe {
        GetOverlappedResult(
            handle.as_raw_handle() as HANDLE,
            overlapped.as_mut(),
            &mut number_of_bytes_transferred,
            /* bWait */ TRUE,
        )
    };

    if res == FALSE {
        // We can't wait for the cancelling indefinitely, so leak the
        // OVERLAPPED structure instead.
        let _ = Box::leak(overlapped);
    }
}
