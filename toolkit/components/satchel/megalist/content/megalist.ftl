# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

filter-input =
  .placeholder = Search passwords
  .key = F
  .aria-label = Search passwords

menu-more-options-button =
  .title = More options

more-options-popup =
  .aria-label = More Options

## Commands

command-copy = Copy
command-reveal = Reveal
command-conceal = Conceal
command-toggle = Toggle
command-open = Open
command-delete = Remove record
command-edit = Edit
command-save = Save
command-cancel = Cancel

## Passwords

passwords-section-label = Passwords
passwords-disabled = Passwords are disabled

passwords-expand-section-tooltip = Show Passwords
passwords-collapse-section-tooltip = Hide Passwords

passwords-dismiss-breach-alert-command = Dismiss breach alert
passwords-command-create = Add Password
passwords-command-import-from-browser = Import from Another Browser…
passwords-command-import = Import from a File…
passwords-command-export = Export Passwords…
passwords-command-remove-all = Remove All Passwords…
passwords-command-settings = Settings
passwords-command-help = Help
passwords-command-sort-name = Sort By Name (A-Z)
# This message can be seen when sorting logins by breached or vulnerable alerts.
passwords-command-sort-alerts = Sort By Alerts

passwords-os-auth-dialog-caption = { -brand-full-name }

# This message can be seen when attempting to export a password in about:logins on Windows.
passwords-export-os-auth-dialog-message-win = To export your passwords, enter your Windows login credentials. This helps protect the security of your accounts.
# This message can be seen when attempting to export a password in about:logins
# The macOS strings are preceded by the operating system with "Firefox is trying to "
# and includes subtitle of "Enter password for the user "xxx" to allow this." These
# notes are only valid for English. only provide the reason that account verification is needed. Do not put a complete sentence here.
passwords-export-os-auth-dialog-message-macosx = export saved passwords

passwords-import-file-picker-title = Import Passwords
passwords-import-file-picker-import-button = Import

# A description for the .csv file format that may be shown as the file type
# filter by the operating system.
passwords-import-file-picker-csv-filter-title =
  { PLATFORM() ->
      [macos] CSV Document
     *[other] CSV File
  }
# A description for the .tsv file format that may be shown as the file type
# filter by the operating system. TSV is short for 'tab separated values'.
passwords-import-file-picker-tsv-filter-title =
  { PLATFORM() ->
      [macos] TSV Document
     *[other] TSV File
  }

# Title of the file picker dialog
passwords-export-file-picker-title = Export Passwords from { -brand-short-name }
# The default file name shown in the file picker when exporting saved logins.
# The resultant filename will end in .csv (added in code).
passwords-export-file-picker-default-filename = passwords
passwords-export-file-picker-export-button = Export
# A description for the .csv file format that may be shown as the file type
# filter by the operating system.
passwords-export-file-picker-csv-filter-title =
  { PLATFORM() ->
      [macos] CSV Document
     *[other] CSV File
  }

# Variables
#   $count (number) - Number of passwords
passwords-count =
  { $count ->
      [one] { $count } password
     *[other] { $count } passwords
  }

# Variables
#   $count (number) - Number of filtered passwords
#   $total (number) - Total number of passwords
passwords-filtered-count =
  { $total ->
      [one] { $count } of { $total } password
     *[other] { $count } of { $total } passwords
  }

# Confirm the removal of all saved passwords
#   $total (number) - Total number of passwords
passwords-remove-all-title =
  { $total ->
     [one] Remove { $total } password?
    *[other] Remove all { $total } passwords?
  }

# Checkbox label to confirm the removal of saved passwords
#   $total (number) - Total number of passwords
passwords-remove-all-confirm =
  { $total ->
     [1] Yes, remove password
    *[other] Yes, remove passwords
  }

# Button label to confirm removal of saved passwords
passwords-remove-all-confirm-button = Confirm

# Message to confirm the removal of saved passwords
#   $total (number) - Total number of passwords
passwords-remove-all-message =
  { $total ->
     [1] This will remove your saved password and any breach alerts. You cannot undo this action.
    *[other] This will remove your saved passwords and any breach alerts. You cannot undo this action.
  }

passwords-origin-label = Website
# The attribute .data-after describes the text that should be displayed for the ::after pseudo-selector
passwords-username-label = Username
  .data-after = Copied
# The attribute .data-after describes the text that should be displayed for the ::after pseudo-selector
passwords-password-label = Password
  .data-after = Copied

passwords-radiogroup-label =
  .aria-label = Filter passwords

# Radiobutton label to display total number of passwords
#   $total (number) - Total number of passwords
passwords-radiobutton-all = All ({ $total })

# Radiobutton label to display total number of alerts
#   $total (number) - Total number of alerts
passwords-radiobutton-alerts = Alerts ({ $total })

## Login Form

passwords-create-label =
  .label = Add password
passwords-edit-label =
  .label = Edit password
passwords-origin-tooltip = Enter the exact address where you’ll sign in to this site.
passwords-username-tooltip = Enter the username, email address, or account number you use to sign in.
passwords-password-tooltip = Enter the password used to sign in to this account.

website-icon =
  .alt = Website Icon
copy-icon =
  .alt = Copy
check-icon =
  .alt = Copied
alert-icon =
  .alt = Warning

# Variables
#   $url (string) - The url associated with the login
origin-login-line =
  .aria-label = Visit { $url }
  .title = Visit { $url }
# Variables
#   $username (string) - The username associated with the login
username-login-line =
  .aria-label = Copy Username { $username }
  .title = Copy Username { $username }
password-login-line =
  .aria-label = Copy Password
  .title = Copy Password
edit-login-button = Edit
  .tooltiptext = Edit Password

show-password-button =
  .aria-label = Show Password
  .title = Show Password
hide-password-button =
  .aria-label = Hide Password
  .title = Hide Password

## Payments

payments-command-create = Add Payment Method

payments-section-label = Payment methods
payments-disabled = Payments methods are disabled

payments-expand-section-tooltip = Show Payments
payments-collapse-section-tooltip = Hide Payments

# Variables
#   $count (number) - Number of payment methods
payments-count =
  { $count ->
      [one] { $count } payment method
     *[other] { $count } payment methods
  }

# Variables
#   $count (number) - Number of filtered payment methods
#   $total (number) - Total number of payment methods
payments-filtered-count =
  { $total ->
      [one] { $count } of { $total } payment method
     *[other] { $count } of { $total } payment methods
  }

card-number-label = Card Number
card-expiration-label = Expires on
card-holder-label = Name on Card

## Addresses

addresses-command-create = Add Address

addresses-section-label = Addresses
addresses-disabled = Addresses are disabled

addresses-expand-section-tooltip = Show Addresses
addresses-collapse-section-tooltip = Hide Addresses

# Variables
#   $count (number) - Number of addresses
addresses-count =
  { $count ->
      [one] { $count } address
     *[other] { $count } addresses
  }

# Variables
#   $count (number) - Number of filtered addresses
#   $total (number) - Total number of addresses
addresses-filtered-count =
  { $total ->
      [one] { $count } of { $total } address
     *[other] { $count } of { $total } addresses
  }

address-name-label = Name
address-phone-label = Phone
address-email-label = Email
