# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

profile-window-title-2 = { -brand-short-name } - Choose a profile
profile-window-logo =
    .alt = { -brand-short-name } logo
profile-window-heading = Choose a { -brand-short-name } profile
profile-window-body = Keep your work and personal browsing, including things like passwords and bookmarks, totally separate. Or create profiles for everyone who uses this device.
# This checkbox appears in the Choose profile window that appears when the browser is opened. "Show this" refers to this window, which is displayed when the checkbox is enabled.
profile-window-checkbox-label-2 =
    .label = Choose a profile when { -brand-short-name } opens
# This subcopy appears below the checkbox when it is unchecked
profile-window-checkbox-subcopy = { -brand-short-name } will open to your most recently used profile.
profile-window-create-profile = Create a profile
profile-card-edit-button =
    .title = Edit profile
    .aria-label = Edit profile
profile-card-delete-button =
    .title = Delete profile
    .aria-label = Delete profile

# Variables
#   $profileName (string) - The name of the profile
profile-card =
    .title = Open { $profileName }
    .aria-label = Open { $profileName }

# Variables
#   $number (number) - The number of the profile
default-profile-name = Profile { $number }

# The word 'original' is used in the sense that it is the initial or starting profile when you install Firefox.
original-profile-name = Original profile

edit-profile-page-title = Edit profile
edit-profile-page-header = Edit your profile
edit-profile-page-profile-name-label = Profile name
edit-profile-page-theme-header-2 =
    .label = Theme
edit-profile-page-explore-themes = Explore more themes
edit-profile-page-avatar-header-2 =
    .label = Avatar
edit-profile-page-delete-button =
    .label = Delete

edit-profile-page-avatar-selector-opener-link = Edit
avatar-selector-icon-tab = Icon
avatar-selector-custom-tab = Custom
avatar-selector-cancel-button =
  .label = Cancel
avatar-selector-save-button =
  .label = Save
avatar-selector-upload-file = Upload a file
avatar-selector-drag-file = Or drag a file here

edit-profile-page-no-name = Name this profile to help you find it later. Rename it any time.
edit-profile-page-duplicate-name = Profile name already in use. Try a new name.

edit-profile-page-profile-saved = Saved

new-profile-page-title = New profile
new-profile-page-header = Customize your new profile
new-profile-page-header-description = Each profile keeps its unique browsing history and settings separate from your other profiles. Plus, { -brand-short-name }’s strong privacy protections are on by default.
new-profile-page-learn-more = Learn more
new-profile-page-input-placeholder =
    .placeholder = Pick a name like “Work” or “Personal”
new-profile-page-done-button =
    .label = Done editing

## Delete profile dialogue that allows users to review what they will lose if they choose to delete their profile. Each item (open windows, etc.) is displayed in a table, followed by a column with the number of items.

# Variables
#   $profilename (String) - The name of the profile.
delete-profile-page-title = Delete { $profilename } profile

# Variables
#   $profilename (String) - The name of the profile.
delete-profile-header = Delete { $profilename } profile?
delete-profile-description = { -brand-short-name } will permanently delete the following data from this device:
# Open is an adjective, as in "browser windows currently open".
delete-profile-windows = Open windows
# Open is an adjective, as in "browser tabs currently open".
delete-profile-tabs = Open tabs
delete-profile-bookmarks = Bookmarks
delete-profile-history = History (visited pages, cookies, site data)
delete-profile-autofill = Autofill data (addresses, payment methods)
delete-profile-logins = Passwords

##

# Button label
delete-profile-cancel = Cancel
# Button label
delete-profile-confirm = Delete

## These strings are color themes available to select from the profile selection screen. Theme names should be localized.

# This light theme features sunny colors such as goldenrod and pale yellow. Its name evokes the color of a marigold flower. This name can be translated directly if it's easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-marigold-theme-2 = Marigold yellow

# This light theme features various shades of soft, muted purples. Its name evokes the color of a lavender flower. This name can be translated directly if it's easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-lavender-theme-2 = Pale lavender

# This light theme features very pale green tones. Its name evokes the color of pale green mint ice cream. This name can be translated directly if it's easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-lichen-theme-2 = Minty green

# This light theme features various shades of pink ranging from pale to bold. Its name evokes the color of a pink magnolia flower. This name can be translated directly if it's easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-magnolia-theme-2 = Magnolia pink

# Ocean blue is a dark theme that features very dark blues and black. Its name evokes the color of the deep ocean water. This name can be translated directly if it's easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-ocean-theme-2 = Ocean blue

# This dark theme features warm oranges, dark mahogany browns, and earthy red/brown colors. The name evokes the earthy colors of brick masonry. This name can be translated directly if it's easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-terracotta-theme-2 = Brick red

# This dark theme features forest green, dusky green with a gray undertone, and a muted sage green. Its name evokes the rich color of green moss in the forest. This name can be translated directly if its easily understood in your language, or adapted to a more natural sounding name that fits the color scheme.
profiles-moss-theme-2 = Moss green

# The default light theme
profiles-light-theme = Light

# The default dark theme
profiles-dark-theme = Dark

# The default system theme
profiles-system-theme = System

## Alternative text for default profile icons

barbell-avatar-alt =
    .alt = Barbell
bike-avatar-alt =
    .alt = Bike
book-avatar-alt =
    .alt = Book
briefcase-avatar-alt =
    .alt = Briefcase
# Canvas refers to an artist's painting canvas, not the general material
canvas-avatar-alt =
    .alt = Canvas
# Craft refers to hobby arts and crafts, represented by a button/fastener commonly found on clothing like shirts
craft-avatar-alt =
    .alt = Craft
custom-avatar-alt = Custom
# Default favicon refers to the generic globe/world icon that appears in browser tabs when a website doesn't have its own favicon.
default-favicon-avatar-alt =
    .alt = Default favicon
# Diamond refers to the precious stone, not the geometric shape
diamond-avatar-alt =
    .alt = Diamond
flower-avatar-alt =
    .alt = Flower
folder-avatar-alt =
    .alt = Folder
hammer-avatar-alt =
    .alt = Hammer
heart-avatar-alt =
    .alt = Heart
heart-rate-avatar-alt =
    .alt = Heart rate
history-avatar-alt =
    .alt = History
leaf-avatar-alt =
    .alt = Leaf
lightbulb-avatar-alt =
    .alt = Lightbulb
makeup-avatar-alt =
    .alt = Makeup
# Message refers to a text message, not a traditional letter/envelope message
message-avatar-alt =
    .alt = Message
musical-note-avatar-alt =
    .alt = Musical note
palette-avatar-alt =
    .alt = Palette
paw-print-avatar-alt =
    .alt = Paw print
plane-avatar-alt =
    .alt = Plane
# Present refers to a gift box, not the current time period
present-avatar-alt =
    .alt = Present
shopping-avatar-alt =
    .alt = Shopping cart
soccer-avatar-alt =
    .alt = Soccer
sparkle-single-avatar-alt =
    .alt = Sparkle
star-avatar-alt =
    .alt = Star
video-game-controller-avatar-alt =
    .alt = Video game controller

## Labels for default avatar icons

barbell-avatar = Barbell
bike-avatar = Bike
book-avatar = Book
briefcase-avatar = Briefcase
# Canvas refers to an artist's painting canvas, not the general material
canvas-avatar = Canvas
# Craft refers to hobby arts and crafts, represented by a button/fastener commonly found on clothing like shirts
craft-avatar = Craft
custom-avatar = Custom avatar
# Default favicon refers to the generic globe/world icon that appears in browser tabs when a website doesn't have its own favicon.
default-favicon-avatar = Default favicon
# Diamond refers to the precious stone, not the geometric shape
diamond-avatar = Diamond
flower-avatar = Flower
folder-avatar = Folder
hammer-avatar = Hammer
heart-avatar = Heart
heart-rate-avatar = Heart rate
history-avatar = History
leaf-avatar = Leaf
lightbulb-avatar = Lightbulb
makeup-avatar = Makeup
# Message refers to a text message, not a traditional letter/envelope message
message-avatar = Message
musical-note-avatar = Musical note
palette-avatar = Palette
paw-print-avatar = Paw print
plane-avatar = Plane
# Present refers to a gift box, not the current time period
present-avatar = Present
shopping-avatar = Shopping cart
soccer-avatar = Soccer
sparkle-single-avatar = Sparkle
star-avatar = Star
video-game-controller-avatar = Video game controller
