/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{nsresult, NS_OK};
use nsstring::{nsAString, nsString};
use thin_vec::ThinVec;
use windows::core::HSTRING;
use windows::Foundation::Collections::IVectorView;
use windows::UI::Notifications::{ToastNotification, ToastNotificationManager};
use xpcom::{xpcom, xpcom_method};

#[xpcom(implement(nsIAlertsServiceRust), nonatomic)]
struct AlertsServiceRust {}

impl AlertsServiceRust {
    xpcom_method!(get_history => GetHistory(aumid: *const nsAString, result: *mut ThinVec<nsString>));
    fn get_history(
        &self,
        aumid: &nsAString,
        result: *mut ThinVec<nsString>,
    ) -> Result<(), nsresult> {
        if result == std::ptr::null_mut() {
            return Err(nserror::NS_ERROR_INVALID_ARG);
        }

        // SAFETY: The caller is responsible to pass a valid pointer.
        let result = unsafe { &mut *result };
        || -> windows::core::Result<()> {
            let history = ToastNotificationManager::History()?;
            let notifications: IVectorView<ToastNotification> =
                history.GetHistoryWithId(&HSTRING::from_wide(&aumid[..])?)?;

            for n in notifications {
                let tag = n.Tag()?;
                result.push((&tag.to_string()).into());
            }
            Ok(())
        }()
        .map_err(|_| nserror::NS_ERROR_UNEXPECTED)
    }
}

#[no_mangle]
pub extern "C" fn new_windows_alerts_service(
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nsresult {
    let service = AlertsServiceRust::allocate(InitAlertsServiceRust {});
    // SAFETY: The caller is responsible to pass a valid IID and pointer-to-pointer.
    unsafe { service.QueryInterface(iid, result) }
}
