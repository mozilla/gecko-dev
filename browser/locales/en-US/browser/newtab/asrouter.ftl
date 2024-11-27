# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These messages are used as headings in the recommendation doorhanger

cfr-doorhanger-extension-heading = Recommended Extension
cfr-doorhanger-feature-heading = Recommended Feature

##

cfr-doorhanger-extension-sumo-link =
  .tooltiptext = Why am I seeing this

cfr-doorhanger-extension-cancel-button = Not Now
  .accesskey = N

cfr-doorhanger-extension-ok-button = Add Now
  .accesskey = A

cfr-doorhanger-extension-manage-settings-button = Manage Recommendation Settings
  .accesskey = M

cfr-doorhanger-extension-never-show-recommendation = Don’t Show Me This Recommendation
  .accesskey = S

cfr-doorhanger-extension-learn-more-link = Learn more

# This string is used on a new line below the add-on name
# Variables:
#   $name (String) - Add-on author name
cfr-doorhanger-extension-author = by { $name }

# This is a notification displayed in the address bar.
# When clicked it opens a panel with a message for the user.
cfr-doorhanger-extension-notification = Recommendation
# .a11y-announcement is extracted in JS and announced via A11y.announce.
cfr-doorhanger-extension-notification2 = Recommendation
  .tooltiptext = Extension recommendation
  .a11y-announcement = Extension recommendation available

# This is a notification displayed in the address bar.
# When clicked it opens a panel with a message for the user.
# .a11y-announcement is extracted in JS and announced via A11y.announce.
cfr-doorhanger-feature-notification = Recommendation
  .tooltiptext = Feature recommendation
  .a11y-announcement = Feature recommendation available

## Add-on statistics
## These strings are used to display the total number of
## users and rating for an add-on. They are shown next to each other.

# Variables:
#   $total (Number) - The rating of the add-on from 1 to 5
cfr-doorhanger-extension-rating =
  .tooltiptext =
    { $total ->
        [one] { $total } star
       *[other] { $total } stars
    }
# Variables:
#   $total (Number) - The total number of users using the add-on
cfr-doorhanger-extension-total-users =
  { $total ->
      [one] { $total } user
     *[other] { $total } users
  }

## Firefox Accounts Message

cfr-doorhanger-bookmark-fxa-header = Sync your bookmarks everywhere.
cfr-doorhanger-bookmark-fxa-body-2 = Great find! Now don’t be left without this bookmark on your mobile devices. Get started with an account.
cfr-doorhanger-bookmark-fxa-link-text = Sync bookmarks now…
cfr-doorhanger-bookmark-fxa-close-btn-tooltip =
  .aria-label = Close button
  .title = Close

## What's New toolbar button and panel

# This string is used by screen readers to offer a text based alternative for
# the notification icon
cfr-badge-reader-label-newfeature = New feature:

cfr-whatsnew-button =
  .label = What’s New
  .tooltiptext = What’s New

cfr-whatsnew-release-notes-link-text = Read the release notes

## Enhanced Tracking Protection Milestones

# Variables:
#   $blockedCount (Number) - The total count of blocked trackers. This number will always be greater than 1.
#   $date (Datetime) - The date we began recording the count of blocked trackers
cfr-doorhanger-milestone-heading2 =
  { $blockedCount ->
    *[other] { -brand-short-name } blocked over <b>{ $blockedCount }</b> trackers since { DATETIME($date, month: "long", year: "numeric") }!
  }
cfr-doorhanger-milestone-ok-button = See All
  .accesskey = S
cfr-doorhanger-milestone-close-button = Close
  .accesskey = C

## DOH Message

cfr-doorhanger-doh-body = Your privacy matters. { -brand-short-name } now securely routes your DNS requests whenever possible to a partner service to protect you while you browse.
cfr-doorhanger-doh-header = More secure, encrypted DNS lookups
cfr-doorhanger-doh-primary-button-2 = Okay
  .accesskey = O
cfr-doorhanger-doh-secondary-button = Disable
  .accesskey = D

## Full Video Support CFR message

cfr-doorhanger-video-support-body = Videos on this site may not play correctly on this version of { -brand-short-name }. For full video support, update { -brand-short-name } now.
cfr-doorhanger-video-support-header = Update { -brand-short-name } to play video
cfr-doorhanger-video-support-primary-button = Update Now
  .accesskey = U

## VPN promotion dialog for public Wi-Fi users
##
## If a user is detected to be on a public Wi-Fi network, they are given a
## bit of info about how to improve their privacy and then offered a button
## to the Mozilla VPN page and a link to dismiss the dialog.

# This header text can be explicitly wrapped.
spotlight-public-wifi-vpn-header = Looks like you’re using public Wi-Fi
spotlight-public-wifi-vpn-body = To hide your location and browsing activity, consider a Virtual Private Network. It will help keep you protected when browsing in public places like airports and coffee shops.
spotlight-public-wifi-vpn-primary-button = Stay private with { -mozilla-vpn-brand-name }
  .accesskey = S
spotlight-public-wifi-vpn-link = Not Now
  .accesskey = N

## Emotive Continuous Onboarding

spotlight-better-internet-header = A better internet starts with you
spotlight-better-internet-body = When you use { -brand-short-name}, you’re voting for an open and accessible internet that’s better for everyone.
spotlight-peace-mind-header = We’ve got you covered
spotlight-peace-mind-body = Every month, { -brand-short-name } blocks an average of over 3,000 trackers per user. Because nothing, especially privacy nuisances like trackers, should stand between you and the good internet.
spotlight-pin-primary-button = { PLATFORM() ->
    [macos] Keep in Dock
   *[other] Pin to taskbar
}
spotlight-pin-secondary-button = Not now

## MR2022 Background Update Windows native toast notification strings.
##
## These strings will be displayed by the Windows operating system in
## a native toast, like:
##
## <b>multi-line title</b>
## multi-line text
## <img>
## [ primary button ] [ secondary button ]
##
## The button labels are fitted into narrow fixed-width buttons by
## Windows and therefore must be as narrow as possible.

mr2022-background-update-toast-title = New { -brand-short-name }. More private. Fewer trackers. No compromises.
mr2022-background-update-toast-text = Try the newest { -brand-short-name } now, upgraded with our strongest anti-tracking protection yet.

# This button label will be fitted into a narrow fixed-width button by
# Windows. Try to not exceed the width of the English text (compare it
# using a variable font like Arial): the button can only fit 1-2
# additional characters, exceeding characters will be truncated.
mr2022-background-update-toast-primary-button-label = Open { -brand-shorter-name } Now

# This button label will be fitted into a narrow fixed-width button by
# Windows. Try to not exceed the width of the English text (compare it using a
# variable font like Arial): the button can only fit 1-2 additional characters,
# exceeding characters will be truncated.
mr2022-background-update-toast-secondary-button-label = Remind Me Later

## Cookie Banner Handling CFR

cookie-banner-blocker-onboarding-header = { -brand-short-name } just refused a cookie banner for you
cookie-banner-blocker-onboarding-body = Less distractions, less cookies tracking you on this site.
cookie-banner-blocker-onboarding-learn-more = Learn more

## These strings are used in the Fox doodle Pin/set default spotlights

july-jam-headline = We’ve got you covered
july-jam-body = Every month, { -brand-short-name } blocks an average of 3,000+ trackers per user, giving you safe, speedy access to the good internet.
july-jam-set-default-primary = Open my links with { -brand-short-name }
fox-doodle-pin-headline = Welcome back

# “indie” is short for the term “independent”.
# In this instance, free from outside influence or control.
fox-doodle-pin-body = Here’s a quick reminder that you can keep your favorite indie browser just one click away.
fox-doodle-pin-primary = Open my links with { -brand-short-name }
fox-doodle-pin-secondary = Not now

## These strings are used in the Set Firefox as Default PDF Handler for Existing Users experiment

set-default-pdf-handler-headline = <strong>Your PDFs now open in { -brand-short-name }.</strong> Edit or sign forms directly in your browser. To change, search “PDF” in settings.
set-default-pdf-handler-primary = Got it

## FxA sync CFR

fxa-sync-cfr-header = New device in your future?
fxa-sync-cfr-body = Make sure your latest bookmarks, passwords, and tabs come with you any time you open a new { -brand-product-name } browser.
fxa-sync-cfr-primary = Learn more
  .accesskey = L
fxa-sync-cfr-secondary = Remind me later
  .accesskey = R

## Device Migration FxA Spotlight

device-migration-fxa-spotlight-heavy-user-header = Don’t forget to back up your data
device-migration-fxa-spotlight-heavy-user-body = Make sure important info — like bookmarks and passwords — is updated and protected across all your devices.
device-migration-fxa-spotlight-heavy-user-primary-button = Get started

device-migration-fxa-spotlight-older-device-header = Peace of mind, from { -brand-product-name }
device-migration-fxa-spotlight-older-device-body = An account keeps your important info updated and protected on any device you connect.
device-migration-fxa-spotlight-older-device-primary-button = Create an account

device-migration-fxa-spotlight-getting-new-device-header-2 = New device in your future?
device-migration-fxa-spotlight-getting-new-device-body-2 = Follow a few simple steps to bring your bookmarks, history, and passwords with you when you get started on a new device.
device-migration-fxa-spotlight-getting-new-device-primary-button = How to back up my data

device-migration-fxa-spotlight-sync-header = Browse without missing a beat
device-migration-fxa-spotlight-sync-body = Sync all your important info with encryption — like bookmarks and passwords. You can pull everything up anywhere you use { -brand-product-name }.
device-migration-fxa-spotlight-sync-primary-button = Get started

## Set as Default PDF Reader Infobar

# The question portion of the following message should have the <strong> and </strong> tags surrounding it.
pdf-default-notification-message = <strong>Make { -brand-short-name } your default PDF reader?</strong> Use { -brand-short-name } to read and edit PDFs saved to your computer.
pdf-default-notification-set-default-button =
    .label = Set as default
pdf-default-notification-decline-button =
    .label = Not now

## Launch on login infobar notification

launch-on-login-infobar-message = <strong>Open { -brand-short-name } every time you restart your computer?</strong> Now you can set { -brand-short-name } to open automatically when you restart your device.
launch-on-login-learnmore = Learn more
launch-on-login-infobar-confirm-button = Yes, open { -brand-short-name }
  .accesskey = Y
launch-on-login-infobar-reject-button = Not now
  .accesskey = N

## These string variants are used when the “launch on login” infobar
## notification is displayed for a second time.

launch-on-login-infobar-final-message = <strong>Open { -brand-short-name } every time you restart your computer?</strong> To manage your Startup preferences, search “startup” in settings.
launch-on-login-infobar-final-reject-button = No thanks
  .accesskey = N

## Tail Fox Set Default Spotlight

# This title is displayed together with the picture of a running fox with a long tail.
# In English, this is a figure of speech meaning 'stop something from following you'.
# If the localization of this message is challenging, consider using a simplified
# alternative as a reference for translation: 'Keep unwanted trackers away'.
tail-fox-spotlight-title = Keep pesky trackers off your tail
tail-fox-spotlight-subtitle = Say goodbye to annoying ad trackers and settle into a safer, speedy internet experience.
tail-fox-spotlight-primary-button = Open my links with { -brand-short-name }
tail-fox-spotlight-secondary-button = Not now

## Root Certificate Succession Infobar

root-certificate-succession-infobar-january-message = <strong>Older versions of { -brand-short-name } may start to have problems on January 14, 2025.</strong>
root-certificate-succession-infobar-march-message = <strong>Update to keep using { -brand-short-name } after March 14, 2025.</strong>
root-certificate-succession-infobar-link = Why do I need to update?
root-certificate-succession-infobar-primary-button =
  .label = Update now
  .accesskey = U
root-certificate-succession-infobar-secondary-button =
  .label = Later
  .accesskey = L

## Root Certificate Succession Windows Background Notification

root-certificate-windows-background-notification-title = You missed an important { -brand-short-name } update
root-certificate-windows-background-notification-subtitle = Parts of the browser will stop working soon if you don’t update. Now is a great time to get our latest protections and features.
root-certificate-windows-background-notification-learn-more-button = Learn more
root-certificate-windows-background-notification-update-button = Update { -brand-short-name }

## FxA Menu Message variants

fxa-menu-message-close-button =
    .title = Close
    .aria-label = Close

fxa-menu-message-sign-up-button = Sign up

fxa-menu-message-sync-devices-primary-text = Sync all your devices
fxa-menu-message-sync-devices-secondary-text = Instantly get your info — like bookmarks and passwords — everywhere you use { -brand-short-name }.
fxa-menu-message-sync-devices-collapsed-text = Sync all your devices

fxa-menu-message-backup-data-primary-text = Back up your browser data
fxa-menu-message-backup-data-secondary-text = Automatically protect bookmarks, passwords, and other info on all your devices.
fxa-menu-message-backup-data-collapsed-text = Back up browser data

fxa-menu-message-backup-sync-primary-text = Keep your data safe and synced
fxa-menu-message-backup-sync-secondary-text = Sync backs up most of your data so you can access it everywhere you use { -brand-short-name }.
fxa-menu-message-backup-sync-collapsed-text = Sync and back up data

fxa-menu-message-mobile-primary-text = Send tabs to your phone
fxa-menu-message-mobile-secondary-text = Pick up where you left off instantly when you sync your tabs with a mobile device.
fxa-menu-message-mobile-collapsed-text = Sync with your phone

## Multi-CTA Fox Doodle Spotlight

multi-cta-fox-doodle-title = Welcome back

multi-cta-fox-doodle-set-default-checkbox = Set { -brand-short-name } to default
multi-cta-fox-doodle-pin-startmenu-checkbox = Pin { -brand-short-name } to Start menu
multi-cta-fox-doodle-pin-checkbox  = { PLATFORM() ->
    [macos] Keep { -brand-short-name } in Dock
   *[other] Pin { -brand-short-name } to taskbar
}
multi-cta-fox-doodle-start-browsing-primary-button-label = Start browsing
multi-cta-fox-doodle-main-browser-primary-button-label = Make { -brand-short-name } my main browser

multi-cta-fox-doodle-quick-reminder-subtitle = Here’s a quick reminder that you can keep your favorite privacy-focused browser just one click away.
multi-cta-fox-doodle-privacy-focused-subtitle  = { PLATFORM() ->
    [macos] Keep your favorite privacy-focused browser just one click away. Set { -brand-short-name } as your default browser for opening links and keep it in your Dock.
   *[other] Keep your favorite privacy-focused browser just one click away. Set { -brand-short-name } as your default browser for opening links and pin it to your taskbar.
}
multi-cta-fox-doodle-msix-privacy-focused-subtitle = Keep your favorite privacy-focused browser just one click away. Set { -brand-short-name } as your default browser for opening links and pin it to your taskbar and Start menu.
