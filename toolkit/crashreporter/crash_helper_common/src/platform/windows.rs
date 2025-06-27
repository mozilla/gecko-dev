/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    errors::{IPCError, MessageError},
    Pid, IO_TIMEOUT,
};
use std::{
    mem::zeroed,
    os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle, RawHandle},
    ptr::{null, null_mut},
};
use windows_sys::Win32::{
    Foundation::{
        GetLastError, ERROR_IO_INCOMPLETE, ERROR_IO_PENDING, ERROR_NOT_FOUND, ERROR_PIPE_CONNECTED,
        FALSE, HANDLE, WAIT_TIMEOUT, WIN32_ERROR,
    },
    Storage::FileSystem::{ReadFile, WriteFile},
    System::{
        Pipes::ConnectNamedPipe,
        Threading::{CreateEventA, ResetEvent, SetEvent, INFINITE},
        IO::{CancelIoEx, GetOverlappedResultEx, OVERLAPPED},
    },
};

pub(crate) fn get_last_error() -> WIN32_ERROR {
    // SAFETY: This is always safe to call
    unsafe { GetLastError() }
}

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
        return Err(IPCError::System(get_last_error()));
    }

    // SAFETY: We just verified that `raw_handle` is valid.
    Ok(unsafe { OwnedHandle::from_raw_handle(raw_handle) })
}

fn set_event(handle: HANDLE) -> Result<(), IPCError> {
    // SAFETY: This is always safe, even when passing an invalid handle.
    if unsafe { SetEvent(handle) } == FALSE {
        Err(IPCError::System(get_last_error()))
    } else {
        Ok(())
    }
}

fn cancel_overlapped_io(handle: HANDLE, overlapped: &mut OVERLAPPED) -> bool {
    // SAFETY: the pointer to the overlapped structure is always valid as the
    // structure is passed by reference. The handle should be valid but will
    // be handled properly in case it isn't.
    let res = unsafe { CancelIoEx(handle, overlapped) };
    if res == FALSE {
        if get_last_error() == ERROR_NOT_FOUND {
            // There was no pending operation
            return true;
        }

        return false;
    }

    // Just wait for the operation to finish, we don't care about the result
    let mut number_of_bytes_transferred: u32 = 0;
    // SAFETY: Same as above
    let res = unsafe {
        GetOverlappedResultEx(
            handle,
            overlapped,
            &mut number_of_bytes_transferred,
            INFINITE,
            /* bAlertable */ FALSE,
        )
    };

    res != FALSE
}

pub(crate) struct OverlappedOperation {
    handle: OwnedHandle,
    overlapped: Option<Box<OVERLAPPED>>,
    buffer: Option<Vec<u8>>,
}

enum OverlappedOperationType {
    Read,
    Write,
}

impl OverlappedOperation {
    pub(crate) fn listen(
        handle: OwnedHandle,
        event: HANDLE,
    ) -> Result<OverlappedOperation, IPCError> {
        let mut overlapped = Self::overlapped_with_event(event)?;

        // SAFETY: We guarantee that the handle and OVERLAPPED object are both
        // valid and remain so while used by this function.
        let res =
            unsafe { ConnectNamedPipe(handle.as_raw_handle() as HANDLE, overlapped.as_mut()) };
        let error = get_last_error();

        if res != FALSE {
            // According to Microsoft's documentation this should never happen,
            // we check out of an abundance of caution.
            return Err(IPCError::System(error));
        }

        if error == ERROR_PIPE_CONNECTED {
            // The operation completed synchronously, set the event so that
            // waiting on it will return immediately.
            set_event(event)?;
        } else if error != ERROR_IO_PENDING {
            return Err(IPCError::System(error));
        }

        Ok(OverlappedOperation {
            handle,
            overlapped: Some(overlapped),
            buffer: None,
        })
    }

    pub(crate) fn accept(mut self, handle: HANDLE) -> Result<(), IPCError> {
        let overlapped = self.overlapped.take().unwrap();
        let mut _number_of_bytes_transferred: u32 = 0;
        // SAFETY: The pointer to the OVERLAPPED structure is under our
        // control and thus guaranteed to be valid.
        let res = unsafe {
            GetOverlappedResultEx(
                handle,
                overlapped.as_ref(),
                &mut _number_of_bytes_transferred,
                0,
                /* bAlertable */ FALSE,
            )
        };

        if res == FALSE {
            let error = get_last_error();
            if error == ERROR_IO_INCOMPLETE {
                // The I/O operation did not complete yet
                self.cancel_or_leak(overlapped, None);
            }

            return Err(IPCError::System(error));
        }

        Ok(())
    }

    fn await_io(
        mut self,
        optype: OverlappedOperationType,
        wait: bool,
    ) -> Result<Option<Vec<u8>>, IPCError> {
        let overlapped = self.overlapped.take().unwrap();
        let buffer = self.buffer.take().unwrap();
        let mut number_of_bytes_transferred: u32 = 0;
        // SAFETY: All the pointers passed to this call are under our control
        // and thus guaranteed to be valid.
        let res = unsafe {
            GetOverlappedResultEx(
                self.handle.as_raw_handle() as HANDLE,
                overlapped.as_ref(),
                &mut number_of_bytes_transferred,
                if wait { IO_TIMEOUT as u32 } else { 0 },
                /* bAlertable */ FALSE,
            )
        };

        if res == FALSE {
            let error = get_last_error();
            if (wait && (error == WAIT_TIMEOUT)) || (!wait && (error == ERROR_IO_INCOMPLETE)) {
                // The I/O operation did not complete yet
                self.cancel_or_leak(overlapped, Some(buffer));
            }

            return Err(IPCError::System(error));
        }

        if (number_of_bytes_transferred as usize) != buffer.len() {
            return Err(IPCError::BadMessage(MessageError::InvalidData));
        }

        Ok(match optype {
            OverlappedOperationType::Read => Some(buffer),
            OverlappedOperationType::Write => None,
        })
    }

    pub(crate) fn sched_recv(
        handle: OwnedHandle,
        event: HANDLE,
        expected_size: usize,
    ) -> Result<OverlappedOperation, IPCError> {
        let mut overlapped = Self::overlapped_with_event(event)?;
        let mut buffer = vec![0u8; expected_size];
        let number_of_bytes_to_read: u32 = expected_size.try_into()?;
        // SAFETY: We control all the pointers going into this call, guarantee
        // that they're valid and that they will be alive for the entire
        // duration of the asynchronous operation.
        let res = unsafe {
            ReadFile(
                handle.as_raw_handle() as HANDLE,
                buffer.as_mut_ptr(),
                number_of_bytes_to_read,
                null_mut(),
                overlapped.as_mut(),
            )
        };

        let error = get_last_error();
        if res != FALSE {
            // The operation completed synchronously, set the event so that
            // waiting on it will return immediately.
            set_event(event)?;
        } else if error != ERROR_IO_PENDING {
            return Err(IPCError::System(error));
        }

        Ok(OverlappedOperation {
            handle,
            overlapped: Some(overlapped),
            buffer: Some(buffer),
        })
    }

    pub(crate) fn collect_recv(self, wait: bool) -> Result<Vec<u8>, IPCError> {
        Ok(self.await_io(OverlappedOperationType::Read, wait)?.unwrap())
    }

    pub(crate) fn sched_send(
        handle: OwnedHandle,
        event: HANDLE,
        mut buffer: Vec<u8>,
    ) -> Result<OverlappedOperation, IPCError> {
        let mut overlapped = Self::overlapped_with_event(event)?;
        let number_of_bytes_to_write: u32 = buffer.len().try_into()?;
        // SAFETY: We control all the pointers going into this call, guarantee
        // that they're valid and that they will be alive for the entire
        // duration of the asynchronous operation.
        let res = unsafe {
            WriteFile(
                handle.as_raw_handle() as HANDLE,
                buffer.as_mut_ptr(),
                number_of_bytes_to_write,
                null_mut(),
                overlapped.as_mut(),
            )
        };

        let error = get_last_error();
        if res != FALSE {
            // The operation completed synchronously, set the event so that
            // waiting on it will return immediately.
            set_event(event)?;
        } else if error != ERROR_IO_PENDING {
            return Err(IPCError::System(error));
        }

        Ok(OverlappedOperation {
            handle,
            overlapped: Some(overlapped),
            buffer: Some(buffer),
        })
    }

    pub(crate) fn complete_send(self, wait: bool) -> Result<(), IPCError> {
        self.await_io(OverlappedOperationType::Write, wait)?;
        Ok(())
    }

    fn overlapped_with_event(event: HANDLE) -> Result<Box<OVERLAPPED>, IPCError> {
        // SAFETY: This is always safe, even when passing an invalid handle.
        if unsafe { ResetEvent(event) } == FALSE {
            return Err(IPCError::System(get_last_error()));
        }

        Ok(Box::new(OVERLAPPED {
            hEvent: event,
            ..unsafe { zeroed() }
        }))
    }

    fn cancel_or_leak(&self, mut overlapped: Box<OVERLAPPED>, buffer: Option<Vec<u8>>) {
        if !cancel_overlapped_io(self.handle.as_raw_handle() as HANDLE, overlapped.as_mut()) {
            // If we cannot cancel the operation we must leak the
            // associated buffers so that they're available in case it
            // ever completes.
            Box::leak(overlapped);
            if let Some(buffer) = buffer {
                buffer.leak();
            }
        }
    }
}

impl Drop for OverlappedOperation {
    fn drop(&mut self) {
        let overlapped = self.overlapped.take();
        let buffer = self.buffer.take();
        if let Some(overlapped) = overlapped {
            self.cancel_or_leak(overlapped, buffer);
        }
    }
}
